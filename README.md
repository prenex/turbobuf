# turbobuf
Lightweight half-human-readable, half-low-level communication protocol with tools

Idea
====
The idea is to create an extensible protocol that is easy to implement light-weight parsers for. To get a reasonable performance in optimized and embedded applications, we are using a structure that is closer to binary and is really dense, however still readable to humans. On my little tests on paper, messages are not growing that much when don't need and overhead can be minimized! These messages can go through text-based channels too.

The whole syntax of the representation is easy to parse and type-check. The latter also supports extending the language of the protocol with user defined types easily and getting support for these from the runtime when asking for data. Parsing is trivial, because the structure is just a stream of hex digits that can be organized below tree-nodes with having one special built-in tree-node that contains UTF8 strings. Basically persing goes like this: We must find some identifier (no capitals there so that we are not confusing with the hex digits!) first and a tree-node or the special message node with UTF8. Then after the open brace we search for hex digits as much as they come and we either end this node with a closing brace or we find further identifiers. Because the structure is a well-formed tree, no real parsing need to be done and we can process data at this point if we only implement this part. This solution is great for embedded projects with small protocols where we don't want to check things, nor need to get help when extracting call information.

The syntax of the basic language elements of the protocol mentioned above is written down in tbuf.tbnf in turbo-bnf format. Further sub-languages can be defined in turbo-bnf and use that with the runtime. This is not only great for syntax checking if a sane message have been gotten by us, but this can aid context-aware extraction of data with its better type-informations!

This repository will be used for the development of the ideas, specification and the reference implementation tools.

Hopefully this data-exchange format will help us grow together and do great things in embedded or special computing! Hopefully this will be performant enough for people to use and human readable enough to debug and mock easily with no sophisticated tools. I also intend this format to store tree-based data and do various operations on trees.
