#ifndef TURBO_BUF_H
#define TURBO_BUF_H

#include<cassert> /* For assertions #define TBUF_ASSERT */
#include<memory>
#include<vector>
#include<cstdio>
#include<cstring>
#include<cctype>
#include<functional>
#include<initializer_list>
#include<unordered_set>
#include"fio.h"
#include"tbuf_data.h"

// Uncomment this if we want to see the debug logging
// Or even better: define this before including us...
//#define DEBUG_LOG 1

namespace tbuf {

/** The special tree-node name that contains string-data */
const char SYM_STRING_NODE = '$';
const char SYM_OPEN_NODE = '{';
const char SYM_CLOSE_NODE = '}';
const char SYM_ESCAPE = '\\';	// The "\" is used for escaping in string nodes
const char SYM_COMMENT = '#';	// Comment until the end of line in tbuf files

const char *SYM_STRING_NODE_CLASS_STR = "$_"; // Any $ and $_something nodes!
const char *SYM_STRING_NODE_STR = "$";
const char *SYM_OPEN_NODE_STR = "{";
const char *SYM_CLOSE_NODE_STR = "}";
const char *SYM_ESCAPE_STR = "\\";	// The "\" is used for escaping in string nodes
const char *SYM_COMMENT_STR = "#";	// Comment until the end of line

/** Returns true if the character is one of the newlines */
inline bool isALineEndChar(char c) {
	// We accept both newline and carriage return
	return (c == '\r' || c == '\n');
}

/** A decriptor that defines descending towards one of the child nodes */
struct LevelDescender {
	/**
	 * The name of the node we should descend down. Does not contain '@' or '_' anymore - just the name prefix!
	 * Can be an empty string in which case no element should be found never.
	 */
	std::string targetName;
	/** The index of the descend-target among the ones with targetName as their name.*/
	int targetIndex;
	/** Defines if we have ad-hoc (prefix) polymorphism - basically saying if we search for prefix or full fit */
	bool adHocPolymorph;

	/** Create empty level descender */
	LevelDescender() : targetName{""}, targetIndex{0}, adHocPolymorph{false} {}

	/** Create a level descender with the given data */
	// Should be explicit to avoid surprises when conversions apply
	explicit LevelDescender(std::string _targetName) : 
		targetName{_targetName}, targetIndex{0}, adHocPolymorph{false} {}

	/** Create a level descender with the given data */
	LevelDescender(std::string _targetName, int _targetIndex) : 
		targetName{_targetName}, targetIndex{_targetIndex}, adHocPolymorph{false} {}

	/** Create a level descender with the given data */
	LevelDescender(std::string _targetName, int _targetIndex, bool _adHocPolymorph) : 
		targetName{_targetName}, targetIndex{_targetIndex}, adHocPolymorph{_adHocPolymorph} {}

	/** Create a level descender with the given data */
	// Should be explicit to avoid surprises when conversions apply
	explicit LevelDescender(const char* descriptor_cstr) : targetIndex{0}, adHocPolymorph{false} {
		// FIXME: fix this so that we do not only handle the simple cases!
		targetName = descriptor_cstr; // uses string copy construction from cstr
	}
};

/**
 * The turbo-buf tree node that might be enchanced with traversal, caching or optimization informations for operations.
 * These are what the trees are built out of. Handled through the tree and memory is owned by the tree!!!
 */
struct Node {
	/** Contains the core-data of this node */
	NodeCore core;

	/**
	 * Points to our parent - can be nullptr for implicit root nodes
	 * - MEMORY IS OWNED BY THE TREE! Do not cache this!
	 */
	Node* parent;

	/** The child nodes (if any). Handled by the tree */
	std::vector<Node> children;

	// TODO: Implement per-node hashing for going down the next level based of the name and simple lookup...
	// TODO: Maybe implement some kind of caching or handle prefix-queries efficiently etc...
	
	/** Descend into one of our children designated by the given level descender (or return nullptr if not available) */
	inline Node* descend(LevelDescender ld) {
		int foundIndex = -1;
		for(Node &child : children) {
#ifdef DEBUG_LOG
printf(" -- Trying child with name:%s against target name: %s\n", child.core.name, ld.targetName.c_str());
#endif
			if(!ld.adHocPolymorph) {
				// Simple lookup
				if(!strcmp(child.core.name, ld.targetName.c_str())) {
					// if child node name is not different from descriptor's name
					// then they are the same and we have found a possible children
					// we can return. Only possible because of at-indexing though...
					++foundIndex;	// update at-indexing
					if(foundIndex == ld.targetIndex) {
						// If we have found that child of this name/type
						// we can return pointer to it
						return &child;
					}
				}
			} else {
				// Prefix-lookup
				if(!strncmp(child.core.name, ld.targetName.c_str(), ld.targetName.length())) {
					// if child node name is not different from descriptor's name
					// then they are the same and we have found a possible children
					// we can return. Only possible because of at-indexing though...
					++foundIndex;	// update at-indexing
					if(foundIndex == ld.targetIndex) {
						// If we have found that child of this name/type
						// we can return pointer to it
						return &child;
					}
				}
			}
		}

		// Didn't found any child for this descriptor...
		return nullptr;
	}

	/** A depth-first searching on the sub-tree from this node by visiging all nodes with the given visitor. Ordering is preorder. */
	inline void dfs_preorder(std::function<void (NodeCore &node, unsigned int depth, bool leaf)> visitor) {
		dfs_preorder_impl(visitor, 0);
	}

	/** A depth-first searching on the sub-tree from this node by visiging all nodes with the given visitor. Ordering is postorder. */
	inline void dfs_postorder(std::function<void (NodeCore &node, unsigned int depth, bool leaf)> visitor) {
		dfs_postorder_impl(visitor, 0);
	}

	/** Useful when writing out a subtree below root into a file. Default file is stdout. */
	inline void writeOut(FILE *destFile = stdout, bool prettyPrint = true) {
		// This needs to be shared (in order to properly close the still open nodes in the end)
		unsigned int lastWoDepth = 0; // Last depth

		// Values and variables for proper indicator bits handling across callbacks in the preorder...
		// "VALUES"
		const unsigned int CLEAR_BITS = 0;
		// BITS
		const unsigned int LEAF_BIT = 1;
		const unsigned int EMPTY_DATA_BIT = 2;
		// Indicates stuff: like if on last call we have found a leaf (or not) and if the leaf was empty - useful when closing '}'s!
		unsigned int lastBits = CLEAR_BITS; // Set to CLEAR_BITS because there was no earlier node at the start!!!

		// Write out using a simple DFS - this do everything except closing the last few '}' chars (because calls end)
		// Rem.: prettyPrint and destFile (ptr) can be just a capture by copy,
		//       but the lastWoDepth needs to be changed by the lambda!!!
		this->dfs_preorder([prettyPrint, destFile, &lastWoDepth, &lastBits](tbuf::NodeCore& nc, unsigned int depth, bool leaf){
			// Possibly close earlier node (see that this handles root properly too!)
			// - If the last call was to a leaf, do not do anything however as we close leaves always on the same line!!!
			if((lastBits & LEAF_BIT) == 0) {
				if(prettyPrint && (depth > 0)) fprintf(destFile, "\n");
			} else if(((lastBits & EMPTY_DATA_BIT) != 0) && !prettyPrint) {
				// Insert a space char to 'close' and earlier empty-data leaf
				// This is here to ensure the leaf nodes / "words" with empty data does not "stick together"
				// That is: we need to separate them at least by one space character each otherwise we have problem!!!

				// We only do this if when we get to the same level so there can be more children.
				// If that is not true, we can be sure that we are a children of a parent node
				// and that parent node will be "closed" or is the root so we do not need any more
				// separating ' ' (space) characters in non-pretty-printed cases for closing empty-data leaves!
				if((depth != 0) && (lastWoDepth == depth)) {
					fprintf(destFile, " ");
				}
			}
			bool needIndent = ((lastBits & LEAF_BIT) == 0);	// handle leafs well: close them on the same line simply!
			bool needCloser = ((lastBits & EMPTY_DATA_BIT) == 0); // handle empty-data leafs well: they have no opener!
			while((depth != 0) && (lastWoDepth >= depth)) {
				if(needIndent) {
					if(prettyPrint && (lastWoDepth > 0)) {	// for others, we close on a separate line tabbed well!
						for(unsigned int i = 0; i < lastWoDepth-1; ++i) {
							fprintf(destFile, "\t");
						}
					}
				} else { needIndent = true; } // Only the first closing should happen the same line - others not!!!
				if(needCloser) {
					fprintf(destFile, "}");
				} else { needCloser = true; } // Only the first closer can be omitted as others must have childres (us)
				if(prettyPrint) fprintf(destFile, "\n");
				--lastWoDepth;
			}
			// Indentation
			if(prettyPrint && (depth > 0)) {
				for(unsigned int i = 0; i < depth-1; ++i) {
					fprintf(destFile, "\t");
				}
			}
			// Tree data
			// name is only needed if the depth is non-zero
			if(depth > 0) {
				fprintf(destFile, "%s", nc.name);
				// Omit the opening { for empty-data leaf nodes - they better just written as the name and nothing else
				// - because that is the shortest representation and also makes sense on prettyPrint==true!
				// Rem.: Many times these empty-data leaves act semantically as "words" so it makes semantic sense too!
				//       Words (like forth words and such) don't used to have any '{' and '}' parentheses didn't they?
				bool needOpener = !((nc.nodeKind != NodeKind::TEXT) && (nc.data.isEmpty()) && leaf);
				if(needOpener) {
					fprintf(destFile, "{");
				}
			}
			if(nc.nodeKind != NodeKind::TEXT) {
				// Normal node - show data as uint
				// (if there is any data)
				if(!nc.data.isEmpty()) {
					fprintf(destFile, "%X", nc.data.asUint());
				}
			} else {
				// Text-node - show text
				fprintf(destFile, "%s", nc.text);
			}
			lastWoDepth = depth;

			// Indicate stuff from this run (so the next callback nows)
			lastBits = leaf ? LEAF_BIT : CLEAR_BITS; // Set leafness for the next one
			// Rem.: kind checks are necessary here because we cannot know what is stored in the pointers
			//       otherwise! These are just plain structs with no constructor and uninitialized data!
			if(((nc.nodeKind != NodeKind::TEXT) || (nc.text == nullptr)) &&
			   (((nc.nodeKind != NodeKind::NORM) && (nc.nodeKind != NodeKind::ROOT)) || (nc.data.isEmpty()))){
				lastBits += EMPTY_DATA_BIT;
			}
		});

		// We need to do this here to close the still opened nodes with extra '}' chars!
		// - If the last call was to a leaf, do not do anything however as we close leaves always on the same line!!!
		if((lastBits & LEAF_BIT) == 0) {
			if(prettyPrint && (lastWoDepth > 0)) fprintf(destFile, "\n");
		}
		bool needIndent = ((lastBits & LEAF_BIT) == 0);	// handle leafs well: close them on the same line simply!
		bool needCloser = ((lastBits & EMPTY_DATA_BIT) == 0); // handle empty-data leafs well: they have no opener!
		while(lastWoDepth > 0) {
			if(needIndent) {
				if(prettyPrint && (lastWoDepth > 0)) {
					for(unsigned int i = 0; i < lastWoDepth-1; ++i) {
						fprintf(destFile, "\t");
					}
				}
			} else { needIndent = true; } // Only the first closing should happen the same line - others not!!!
			if(needCloser) {
				fprintf(destFile, "}");
			} else { needCloser = true; } // Only the first closer can be omitted as others must have childres (us)
			if(prettyPrint) fprintf(destFile, "\n");
			--lastWoDepth;
		}
	}

private:
	// Recursive dfs for preorder
	inline void dfs_preorder_impl(std::function<void (NodeCore &node, unsigned int depth, bool leaf)> visitor, unsigned int depth) {
		// Visit
		visitor(this->core, depth, this->children.size() == 0);
		// recurse
		for(int i = 0; i < this->children.size(); ++i) {
			children[i].dfs_preorder_impl(visitor, depth + 1);
		}
	}
	// Recursive dfs for postorder
	inline void dfs_postorder_impl(std::function<void (NodeCore &node, unsigned int depth, bool leaf)> visitor, unsigned int depth) {
		// recurse
		for(int i = 0; i < this->children.size(); ++i) {
			children[i].dfs_preorder_impl(visitor, depth + 1);
		}
		// Visit
		visitor(this->core, depth, this->children.size() == 0);
	}
};

/**
 * Contains static convenience methods to do queries over nodes of trees
 */
class TreeQuery {
public:
	/** Separates levels of the tree in queries */
	const char LEVEL_SEPARATOR = '/';
	/** Describes 'at' relationships - basically describes what fitting result we should get among the many using indexing */
	const char AT_DESCRIPTOR = '@';
	/** The symbol of ad-hoc polymorphism based on prefix matching */
	const char AD_HOC_POLIMORFER= '_';

	// TODO: implement tQuery calls for const char* query strings with '/' as level separator

	/**
	 * Tree-query: Run the given operation on the found node. If node is not found, this will be a NO-OP.
	 * The best and most handy way is to use lambdas when calling a tQuery. When in c++ each "level" of the tree
	 * is handled by one element in the second (initializer list of const char*) parameter. Basically this is the
	 * function that is called when we use the tquery language with only one const char* param and use the '/'
	 * separator between the levels.
	 */
	inline static void fetch(Node &root, std::initializer_list<const char*> tPath, std::function<void (NodeCore &found)> visitor) {
		// Just do the usual call and grab the core from it
		// simple lambda also shows usage as an example.
		fetch(root, tPath, [&visitor] (Node &visited) {
			visitor(visited.core);
		});
	}

	/**
	 * If you do not want to further move along the result in the tree, use the fetches that ask 
	 * for a NodeCore instead in your functor that you are providing! Only use this if you need it!
	 *
	 * Tree-query: Run the given operation on the found node. If node is not found, this will be a NO-OP.
	 * The best and most handy way is to use lambdas when calling a tQuery. When in c++ each "level" of the tree
	 * is handled by one element in the second (initializer list of const char*) parameter. Basically this is the
	 * function that is called when we use the tquery language with only one const char* param and use the '/'
	 * separator between the levels.
	 */
	inline static void fetch(Node &root, std::initializer_list<const char*> tPath, std::function<void (Node &found)> visitor) {
		// Create descenders out of the cstr for each level
		std::vector<LevelDescender> descenders;
		for(const char *pathElem : tPath) {
#ifdef DEBUG_LOG
printf("fetch(...) cstr->descender conversion pathElem: %s\n", pathElem);
#endif
			descenders.push_back(std::move(LevelDescender(pathElem)));
		}
		// Use the fetch already defined for descenders
		fetch(root, std::move(descenders), visitor);
	}

	/**
	 * Tree-query: Run the given operation on the found node. If node is not found, this will be a NO-OP.
	 * The best and most handy way is to use lambdas when calling a tQuery. When in c++ each "level" of the tree
	 * is handled by one element in the second (initializer list of const char*) parameter. Basically this is the
	 * function that is called by the other - more user friendly cases too.
	 */
	inline static void fetch(Node &root, std::vector<LevelDescender> tPath, std::function<void (NodeCore &found)> visitor) {
		// Just do the usual call and grab the core from it
		// simple lambda also shows usage as an example.
		fetch(root, tPath, [&visitor] (Node &visited) {
			visitor(visited.core);
		});
	}

	/**
	 * If you do not want to further move along the result in the tree, use the fetches that ask 
	 * for a NodeCore instead in your functor that you are providing! Only use this if you need it!
	 *
	 * Tree-query: Run the given operation on the found node. If node is not found, this will be a NO-OP.
	 * The best and most handy way is to use lambdas when calling a tQuery. When in c++ each "level" of the tree
	 * is handled by one element in the second (initializer list of const char*) parameter. Basically this is the
	 * function that is called by the other - more user friendly cases too.
	 */
	inline static void fetch(Node &root, std::vector<LevelDescender> tPath, std::function<void (Node &found)> visitor) {
		Node *currentHead = &root;
		for(int i = 0; i < tPath.size(); ++i) {
			// Try descending and update current head with that
			currentHead = currentHead->descend(tPath[i]);
			// Node is not found - exit immediately
			if(currentHead == nullptr) {
				return;
			}
		}

		// Node has been found; return it.
		// No NPE can happen here because of the return-checks above
		visitor(*currentHead);
	}
};

class Tree {
public:
	/** Name for implicit root nodes */
	const char *rootNodeName = "/";	// This name is special as it can be '/' only for the root - see tbnf description!
	//const char *textNodeName = "$"; // This name is special. We use this directly instead of pointing into the buffers...

	/** The root node for this tree */
	Node root;

	/**
	 * Creates and empty tree with a root node that has no children and no data.
	 */
	Tree() {
		// Empty input file, return empty root
		root = Node{NodeKind::ROOT, Hexes{fio::LenString{0,nullptr}}, rootNodeName, nullptr, nullptr, std::vector<Node>()};
	}
	
	/**
	 * Create tree by parsing input.
	 * Might take ownership of data structures of the "input" so that we can parse with optimizations in case
	 * canReferMemoryFromInputLater is set as true. In that case you should really beware that the life-cycle 
	 * of input should be longer than the life-cycle of this object otherwise corrupted strings might get to
	 * be returned from this tree later! If you set that parameter to be true, everything is a little bit
	 * faster as we are not using that much dynamic memory...
	 */
	// TODO: This is not so clean, why not own input when canReferMemoryFromInput is true? Should refactor!?
	template<class InputSubClass>
	Tree(InputSubClass &input, bool canReferMemoryFromInput = false, bool ignoreWhiteSpace = true) {
		// These are only here to ensure type safety
		// in our case of template usage...
		fio::Input *testSubClassing = new InputSubClass();
		delete testSubClassing;	// Should be fast!

		// Parse
		
		// Check if we have any input to parse
		if(input.grabCurr() == EOF) {
			// Empty input file, return empty root
			root = Node{NodeKind::ROOT, Hexes{fio::LenString{0,nullptr}}, rootNodeName, nullptr, nullptr, std::vector<Node>()};
		} else {
			// Properly parse the whole input as a tree
			// Parse hexes for the root node
			Hexes rootHexes = parseHexes(input);
			// Create the root node with empty child lists
			root = Node {NodeKind::ROOT, rootHexes, rootNodeName, nullptr, nullptr, std::vector<Node>()};
			// Fill-in the children while parsing nodes with tree-walking
			parseNodes(input, root, canReferMemoryFromInput, ignoreWhiteSpace);
		}
	}

	/**
	 * Adds a duplicate of the given source node below the specified parent. The src should come from the same tree!
	 *
	 * NodeContent will be shared between the source and the new node after this operation!
	 */
	inline Node& addDuplicate(Node &parent, Node &src) {
#ifdef TBUF_ASSERT
		// Ensure that the parent can have children (not a text node)
		assert(parent.core.nodeKind != NodeKind::TEXT);
		// Ensure that our tree root is on the same tree as the src
		// loop to find the root of src...
		Node* ptr = &src;
		while(ptr->core.nodeKind != NodeKind::ROOT) {
			ptr = ptr->parent;
		}
		// and then assert it is our root too!
		assert(ptr == &root);
#endif
		// Add by setting parent and empty children
		// and of course the very same shared NodeContent data from src
		parent.children.push_back(std::move(Node{src.core, &parent, std::vector<Node>()}));

		return parent.children[parent.children.size() - 1];
	}

	/**
	 * Adds a text node to the tree below the given parent - adding the new child as the latest child insert point.
	 *
	 * The text parameter describes the content and the optional name describes if $_name is used or just "$" alone!
	 */
	inline Node& addTextNode(Node &parent, const std::string &text, const std::string &name = "") {
		// Create the main node-data (NodeCore) that is surely having the "TEXT" kind now
		NodeCore nc;
		nc.nodeKind = NodeKind::TEXT;
		// The name of the node is "$" by default.
		const char *fullName = SYM_STRING_NODE_STR;
		if(name.length() > 0) {
			// Otherwise it is of the form: "$_aUserDefinedName"
			// So we need to add to or lookup string from the treeStrings set
			auto iName = treeStrings.insert(SYM_STRING_NODE_CLASS_STR+name);
			// Refer to this data then (which has tree-bound lifetime)
			fullName = (*(iName.first)).c_str();
		}
		nc.name = fullName;
		// Try adding the text for the node - this also looks up earlier data!
		// Rem.: This ensures there are duplicate data stored except when there are data from first parse c_strs!
		//       That is the latter are not in the treeStrings set because they are still in the morphed input!
		//       (It would take too long to look with linear search in the memory or the tree for char* strings)
		auto iText = treeStrings.insert(text); // iterator for the "text" std::string
		nc.text = (*(iText.first)).c_str();

		// Add a new node below the parent - with the given NodeCore data and pointer to the given parent and no initial children.
		// Rem.: takint address of parent migth be nothing if it is already a reference - if its not things are faster anyways...
		parent.children.push_back(std::move(Node{nc, &parent, std::vector<Node>()}));

		return parent.children[parent.children.size() - 1];
	}

	/**
	 * Adds a normal data node to the tree below the given parent - adding the new child as the latest child insert point.
	 *
	 * Data should contain uppercase [0..9A..F] characters only! Name is necessary in this case and cannot be an empty string!
	 */
	inline Node& addNormalNode(Node &parent, const std::string &data, const std::string &name) {
#ifdef TBUF_ASSERT
		// Ensure preconditions
		// The name must be non-empty
		assert(name.length() > 0);
		// The data must be always a hex character
		for(int i = 0; i < data.length(); ++i) {
			assert(Hexes::isHexCharacter(data[i]));
		}
#endif

		// Create the main node-data (NodeCore) that is surely having the "NORM" kind now
		NodeCore nc;
		nc.nodeKind = NodeKind::NORM;
		// Gather DATA
		nc.data = Hexes::EMPTY_HEXES();
		if(data.length() > 0) {
			// See if we can look up or insert the data as a string
			auto iData = treeStrings.insert(data);
			// Get the length and a pointer properly from the stored string
			// Rem.: Bad conversion is a must here sadly - but it is the only way...
			char* digitStr = (char*)(iData.first->c_str());
			fio::LenString digits = {iData.first->length(), digitStr};
			nc.data = Hexes{digits};
		}
		// Set NAME
		const char *fullName = "missing_node_name"; // This should never show up...
		if(name.length() > 0) {
			// Otherwise it is of the form: "$_aUserDefinedName"
			// So we need to add to or lookup string from the treeStrings set
			auto iName = treeStrings.insert(name);
			// Refer to this data then (which has tree-bound lifetime)
			fullName = (*(iName.first)).c_str();
		}
		nc.name = fullName;

		// Add a new node below the parent - with the given NodeCore data and pointer to the given parent and no initial children.
		// Rem.: takint address of parent migth be nothing if it is already a reference - if its not things are faster anyways...
		parent.children.push_back(std::move(Node{nc, &parent, std::vector<Node>()}));

		return parent.children[parent.children.size() - 1];
	}
private:
	/**
	 * Those strings go here that we are not able to fetch in an optimized way out of the input handler's memory.
	 * Also dynamically added elements strings are going here and user can ask the tree to copy everything instead of
	 * doing the hacky ways of referring to memory we have in the input... Latter is faster, but more error prone.
	 */
	std::unordered_set<std::string> treeStrings;

	/**
	 * Advances input until it is a hex character and parse.
	 * The current of input will point after the first non-hex character after this operation...
	 */
	template<class InputSubClass>
	inline Hexes parseHexes(InputSubClass &input) {
		if(!Hexes::isHexCharacter(input.grabCurr())) {
			// No hexes at current position
			return Hexes {{0, nullptr}};
		} else {
			// Hexes at current position
			void* hexSeam = input.markSeam();
			while(Hexes::isHexCharacter(input.grabCurr())) {
				input.advance();
			}
			fio::LenString digits = input.grabFromSeamToLast(hexSeam);
			return Hexes { digits };
		}
	}

	/** Parse all child nodes of the root after parsing hexes of root */
	template<class InputSubClass>
	inline void parseNodes(InputSubClass &input, Node &parent, bool canReferMemoryFromInput, bool ignoreWhiteSpace){
		// Parse from the very start point after the hexes of the root!
		// (Non-recursive tree-walking in a depth first approach)
		Node* currentParent = &parent;
		while(nullptr != (currentParent = parseNode(input, currentParent, canReferMemoryFromInput, ignoreWhiteSpace)));
	}

	/**
	 * Parse one node and put it as the child node of the referred parent.
	 * returns the pointer to the new parent (we do a depth first tree-walking) or nullptr if we reached EOF!
	 * The new parent can be the child of the parent if we can go deeper, or the parent if we keep the level or
	 * the parent of the parent if we see that have gotten the parents closing parentheses...
	 */
	template<class InputSubClass>
	inline Node* parseNode(InputSubClass &input, Node *parent, bool useOptimizedButHackyStringReferences,bool ignoreWhiteSpace) {
		// Sanity check
		if(parent == nullptr) {
			return nullptr;
		}

		// Try to parse a node which will be saved into the parent
		if(input.grabCurr() == EOF) {
			// Finished parsing
			return nullptr;
		} else if(ignoreWhiteSpace && isspace(input.grabCurr())) {
			// Just advance over whitespaces in most cases 
			// this does not apply when we are in the middle of a text-node however
			input.advance();
			// Return the unchanged parent node as the same
			return parent;
		} else if(input.grabCurr() == SYM_COMMENT) {
			// We have reached a '#' so this line is a comment from now on
			// because we handle comments here and not in an earlier scanner
			// or whatever, the '#' in the string nodes do not start a comment and such!
			// Rem.: We need to take care about what if we run out of input so EOF
			//       check is necessary here too!
			while(!isALineEndChar(input.grabCurr()) && (input.grabCurr() != EOF)) {
				// Advance input
				input.advance();
			}
			// Return the unchanged parent node as the same
			return parent;
		} else if(input.grabCurr() == SYM_STRING_NODE) {
			// Parse '$' symbol tag with string inside - this is always a leaf!
			// advance onto the '{' collecting the node name in-between this
			// way we can have various 'types' or 'variations' of string nodes
			// by adding a type name after the '$' in the protocol!
			void* nameSeamHandle = input.markSeam();	// Mark seam for node name!
			while(input.grabCurr() != SYM_OPEN_NODE) {
				// Input sanity check
				if(input.grabCurr() == EOF) {
					// Completely depleted input: finish parsing
					// happens on badly formatted input...
					// Rem.: We grab from seam here only to ensure mark/grab pairing!
					input.grabFromSeamToLast(nameSeamHandle);
					return nullptr;
				} else {
					// advance to find the node opening
					input.advance();
				}
			}
			// Grab name of the text node
			fio::LenString lsName = input.grabFromSeamToLast(nameSeamHandle);
			char* textNodeName = nullptr;
			if(useOptimizedButHackyStringReferences && input.isSupportingDangerousDestructiveOperations()) {
				// Create null terminated c_str from the LenString
				textNodeName = lsName.dangerous_destructive_unsafe_get_zeroterminator_added_cstr();
			} else {
				// Create std::string from the LenString and
				// store it in the local set of strings we have in the tree.
				// This way the tree owns these copies as it should be.
				treeStrings.insert(lsName.get_str());
				// (!) We need finding this string and setting the pointer to it
				// if I would just set the pointer to an other get_str that
				// would be invalid. We store the std::strings only so that
				// memory release happens properly later!!!
				textNodeName = (char*)(*treeStrings.find(lsName.get_str())).c_str();
			}
#ifdef DEBUG_LOG
printf("(..) Found text-node with name: %s below %s(%u)\n", textNodeName, parent->core.name, (unsigned int)parent);
#endif

			// Let us try to find the contents then...
			fio::LenString content;
			// Go after the '{' - we are now in the inside of the node
			input.advance();
			// This will hold the current character
			char current = input.grabCurr();
			// We mark a seam so that we can accumulate input
			// this should work even if current became EOF immediately!
			void* seamHandle = input.markSeam();
			// Initialize closes flag. EOF surely closes (should not happen though)
			// and also '}' surely closes as no escaping was possible until yet...
			bool nodeClosed = ((current == EOF) || (current == SYM_CLOSE_NODE));
			// Loop and parse the text contents of this node
			while(!nodeClosed) {
				// See if this node is already closed or not
				// here "current" still refers to the last character...
				bool escaped = (current == SYM_ESCAPE);
				input.advance();
				current = input.grabCurr();
				if((current == SYM_CLOSE_NODE) && (!escaped)) {
					// Found the end of the inside of the node
					content = input.grabFromSeamToLast(seamHandle);
					nodeClosed = true;
				}
			}

			// Get the text of this node
			char* text = nullptr;
			if(useOptimizedButHackyStringReferences && input.isSupportingDangerousDestructiveOperations()) {
				// Create null terminated c_str from the LenString
				text = content.dangerous_destructive_unsafe_get_zeroterminator_added_cstr();
				// Support escaping - at least for the '}' character. We need unescaping here...
				content.dangerous_destructive_unsafe_unescape_in_place(SYM_ESCAPE);
			} else {
				// Create std::string from the LenString and
				// store it in the local set of strings we have in the tree.
				// This way the tree owns these copies as it should be.
				// Rem.: unescaping is necssary here too!
				treeStrings.insert(fio::LenString::safe_unescape(SYM_ESCAPE, content.get_str()));
				// (!) We need finding this string and setting the pointer to it
				// if I would just set the pointer to an other get_str that
				// would be invalid. We store the std::strings only so that
				// memory release happens properly later!!!
				text = (char*)(*treeStrings.find(content.get_str())).c_str();
			}
#ifdef DEBUG_LOG
printf("(!) text-node content is: %s below %s(%u)\n", text, parent->core.name, (unsigned int)parent);
#endif

			// Add this new node as our children to the parent
			parent->children.push_back(Node{
					NodeKind::TEXT,
					Hexes {},
					textNodeName,
					text,
					parent,
					std::vector<Node> {}
			});

			// Advance over the '}' closing char
			// (in dangerous operations it became a null terminator anyways)
			input.advance();
			
			// Because the text-only nodes are always leaves
			// the parents stays as it was
			return parent;
		} else if(input.grabCurr() == SYM_CLOSE_NODE) {
			// Parsed the '}' closing symbol
			// Always advance the input when it is not the EOF already!
			input.advance();
			// We should go up one level in the tree walking loop!
			// The only thing we need to do is thus to go upwards
			// if the current parent still had any parent
			if(parent->parent != nullptr) {
				// Go up one level
				return parent->parent;
			} else {
				// Already at top level
				// just stay there and wait for EOF...
				return parent;
			}
		} else {
			// We are parsing a normal node and the read head is on the first letter of the name
			// Parse node name and possibly the node body (the hexes for it)!
			// Also set the newly parsed node as the new current parent by returning it when it is not an empty leaf node!
			
			// Let us try to find the contents - first the node name - then...
			fio::LenString lsNodeName;

			// This will hold the current character
			char current = input.grabCurr();
			// We mark a seam so that we can accumulate input
			// this should work even if current became EOF immediately!
			void* seamHandle = input.markSeam();
			// Initialize closes flag. EOF surely closes (should not happen though)
			bool nodeNameParsed = ((current == EOF));
			// Also we count this as an empty leaf if we have gotten EOF here (should not happen though)
			bool isEmptyLeaf = ((current == EOF));	// This just becomes 'false' in any case the code is not broken!
			// Loop and parse the text contents of this node
			while(!nodeNameParsed) {
				// Advance the input read head
				input.advance();
				// Read the next character
				current = input.grabCurr();
				// When the current char becomes a whitespace that means that this node does not have
				// the '{...}' part and is an empty leaf node. Useful for compactness and when the parser
				// is used for various unusual reasons like parsing forth-like words and such.
				isEmptyLeaf = isspace(current);
				// If we have found the open node '{' char, empty leaf or EOF we reach end of the name
				// when EOF is found that is basically an error that we silently try to handle somehow.
				if((current == SYM_OPEN_NODE) || (current == EOF) || isEmptyLeaf) {
					if(current == EOF) {
						// Syntax error - no opening tag after tag name!
						return nullptr;
					}

					// Found the end of the node name
					lsNodeName = input.grabFromSeamToLast(seamHandle);
					nodeNameParsed = true;
				}
			}

			// Empty leaves does not have the '{' opener, 
			// so do not even try to advance over that in that case!
			if(!isEmptyLeaf) {
				// The read head is on the SYM_OPEN_NODE character now so we need to
				// advance so that we are on the first possible hex-data char...
				input.advance();

				// We should still have data
				if(input.grabCurr() == EOF) {
					// Just another kind of syntax error
					// We just do something so that operation is not undefined..
					return nullptr;
				}
			}

			// Get the text of this node name
			char* nodeName = nullptr;
			// In case of empty leaves we cannot use the optimization as there is no "useless" character
			// for overriding with the '\0' char! If we are not an empty leaf, we can override the '{' safely...
			if(!isEmptyLeaf && useOptimizedButHackyStringReferences && input.isSupportingDangerousDestructiveOperations()) {
				// Create null terminated c_str from the LenString
				// this works as the '{' character will be overridden
				nodeName = lsNodeName.dangerous_destructive_unsafe_get_zeroterminator_added_cstr();
			} else {
				// Create std::string from the LenString and
				// store it in the local set of strings we have in the tree.
				// This way the tree owns these copies as it should be.
				treeStrings.insert(lsNodeName.get_str());
				// (!) We need finding this string and setting the pointer to it
				// if I would just set the pointer to an other get_str that
				// would be invalid. We store the std::strings only so that
				// memory release happens properly later!!!
				nodeName = (char*)(*treeStrings.find(lsNodeName.get_str())).c_str();
			}

			// Empty leafs do not have all the data other cases have
			if(!isEmptyLeaf) {
#ifdef DEBUG_LOG
printf("(!) Found non-empty sub-node with name: %s below %s(%u)\n", nodeName, parent->core.name, (unsigned int)parent);
#endif
				// Now we have all the data to build this subnode
				// and set it as a parent. This must be added as a
				// child of the current parent and returned so that
				// childrens can be read up. If this node is completely
				// empty (like: node{}) this still works the same way!
				parent->children.push_back(Node{
						NodeKind::NORM, // normal node type
						std::move(parseHexes(input)), // inline hexes...
						nodeName, // set the parsed node name
						nullptr, // not a text node
						parent,	// set parent node
						std::vector<Node> {}	// start with empty children - will collect them later!
				});

				// Return the node we just added
				// "vector.back" just returns the last element
				// and we get the address for that as our current
				// parent ptr so that deeper levels have it as parent.
				// This make us do a depth-first tree walking without recursion
				return &parent->children.back();
			} else {
#ifdef DEBUG_LOG
printf("(!) Found an empty-leaf sub-node with name: %s below %s(%u)\n", nodeName, parent->core.name, (unsigned int)parent);
#endif
				// Now we have all the data to build this subnode
				// and set it as a parent. This must be added as a
				// child of the current parent and returned so that
				// childrens can be read up. If this node is completely
				// empty (like: node{}) this still works the same way!
				parent->children.push_back(Node{
						NodeKind::NORM, // normal node type (just no body and child!)
						Hexes::EMPTY_HEXES(), // empty hexes
						nodeName, // set the parsed node name
						nullptr, // not a text node
						parent,	// set parent node
						std::vector<Node> {}	// surely no children - never will be any!
				});
				// We can keep the earlier parent as this node was an empty leaf
				// Further nodes cannot be below an emtpy one of course...
				return parent;
			}
		}
	}
};

} // tbuf namespace ends here
#endif // TURBO_BUF_H
