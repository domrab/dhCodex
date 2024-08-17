# dhCodex
The idea behind the codex is to hand over ownership of custom objects (Things)
to one central authority (the Codex) and use strings containing the UUIDs to
retrieve references.
The Codex maps UUIDs (strings) to Things (custom objects).

Often in programming, circular dependencies provide flexibility to a client but can be quite problematic to manage. To illustrate, take a parent-child relationship as an example. Accessing the parent from a child is convenient, and so is accessing a child through the parent. But unless implemented with care, you may run into memory leaks when the time comes to delete such objects, or other problems stemming from the circular references.
The Codex provides an interface that allows for the creation of circular relationships, composition and aggregation, many-to-many, one-to-many, many-to-one, and one-to-one relationships.
To illustrate, take a parent-child relationship again. The parent can have a member `_children`, containing a list of their UUIDs (strings). And each child can have a member `_parent`, holding a string with the parent's UUID. Both can also have methods `get_children()` and `get_parent()` respectively, which return newly retrieved references from the Codex via the stored UUIDs.

The Codex does not provide a direct implementation for hierarchies since the needs can vary, but it should provide a base to implement your system.
