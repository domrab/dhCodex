"""dhCodex - python - v1.0.0

[LICENSE BELOW]

The idea behind the codex is to hand over ownership of custom objects (Things)
to one central authority (the Codex) and use strings containing the UUIDs to
retrieve references.
The Codex maps UUIDs (str) to Things (object).

While Python (cython at least) is relatively good with circular dependencies,
they should still be generally avoided and can cause problems with many other
systems.
Taking a parent-child relationship as an example, instead of the parent owning
the child, both are owned by the Codex. To make the relationship, the parent
would have a member `sa_children : list[str]` containing the UUIDs of
all of its children. At the same time, the child can have a member
`parent : str` containing the UUID of the parent.
Both can have methods `get_children() -> list[Thing]` and `get_parent() -> Thing`
respectively, retrieving a reference to the respective Things. This method
allows circular relationships, composition and aggregation, many-to-many, one-to-many,
many-to-one and one-to-one relationships.
Thing provides a _on_remove() method which can/should be overwritten in subclasses
to update other nodes with dependencies to the node being deleted. In case of a
parent-child relationship, if the child gets removed, its UUID should be removed
from the parent's children list. Or if the parent gets removed, the child should
be removed as well.

The Codex does not provide a direct implementation for hierarchies since the needs
can vary, but it should provide a base to implement your system.

=====================================================================================

MIT License

Copyright (c) 2023 Dominik Haase

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

=====================================================================================
"""
import uuid


class Thing:
    """
    The base class for all Things. Anything to be added to the Codex should inherit from Thing.
    A Thing provides an UUID that's generated upon initilization of the object and which can
    be queried through `get_uuid()`, as well as a `_on_remove()` method that's called when an
    object gets removed from the Codex as well as in the destructor.
    Removing a Thing from the Codex is similar to deleting it, but since ownership in Python
    is not as strict as it is with unique_ptr in C++, an object may be removed from the index,
    but not immediately deleted.
    """
    def __init__(self):
        """
        The constructor generates a new UUID for this thing and adds it to the Codex. All
        subclasses must call this constructor.
        """
        self._s_uuid = str(uuid.uuid4())
        _add(self)

    def __del__(self):
        """
        As all Things should be managed through the Codex, it should not be necessary to call `_on_remove()`
        in the destructor again. But, better safe than sorry. Someone might create a Thing without calling
        its constructor in which case it would not get added to the Codex.
        Check `_on_remove()` for more details.
        """
        self._on_remove()

    def _on_remove(self):
        """
        This method is similar to the destructor, but instead of being called at the end
        of life of a Thing, it is instead called when a Thing is removed from the Codex.

        For example:
            thing = Thing()\n
            remove(thing)

        Python does not call `__del__()` since `thing` is still holding on to it. So removing
        potential dependencies (eg children) in `__del__()` would be delayed until `thing`
        goes out of scope. This is not what we want since the Codex should no longer contain
        nodes we expect to be deleted.
        `_on_remove()` solves this problem by being explicitly called when an object gets removed.
        This allows for the removal of any dependents as well. While Python itself might hold on
        to the Thing object, it will no longer be accessible through the Codex.
        """
        pass

    def get_uuid(self):
        """
        Getter for the Thing's UUID

        Returns:
            str: the Thing's UUID
        """
        return self._s_uuid


def _get_mapping():
    """
    Returns the mapping dict between UUIDs and their Things.

    This could be implemented in many different ways, but I chose to do what
    I think is the closest to the static local variable implementation in cpp.
    The key is called `.MAPPING` on purpose, so it cannot be accessed through
    `_get_mapping._MAPPING` but only explicitly through _get_mapping.__dict__[...]

    Returns:
        dict: {str: Thing}
    """
    if ".MAPPING" not in _get_mapping.__dict__:
        _get_mapping.__dict__[".MAPPING"] = {}

    return _get_mapping.__dict__[".MAPPING"]


def _add(thing):
    """
    This method is used to add a Thing to the Codex. This method is automatically called
    in the constructor of Thing so there is no need to call this method explicitly in your
    code.
    No check is performed to see if the UUID already exists since UUIDs are, for all
    practical purposes, considered to be unique. If a UUID is already part of the mapping,
    its Thing will be overwritten with whats passed in

    Args:
        thing (Thing): The Thing instance to add

    Returns:
        Thing: Same object that was passed in
    """
    _get_mapping()[thing.get_uuid()] = thing
    return thing


def get(s_uuid):
    """
    This method returns a reference to a Thing from a given UUID

    Args:
        s_uuid (str): The UUID

    Returns:
        Thing|None: The Thing, or if the UUID cannot be found in the mapping, None
    """
    return _get_mapping().get(s_uuid, None)


def get_uuid(thing_or_uuid):
    """
    Convenience method to get a UUID from either a Thing or a UUID.

    Args:
        thing_or_uuid (str|Thing): The Thing or the UUID. If a Thing is provided,
            its UUID is queried. If not, the input gets returned as is

    Returns:
        str: The UUID
    """
    if isinstance(thing_or_uuid, Thing):
        return thing_or_uuid.get_uuid()
    return thing_or_uuid


def get_thing(thing_or_uuid):
    """
    Convenience method to get a Thing from either a Thing or a UUID.

    Args:
        thing_or_uuid (str|Thing): The Thing or the UUID. If a UUID is provided,
            its Thing is queried. If not, the input gets returned as is. If the

    Returns:
        Thing|None: The Thing
    """
    if isinstance(thing_or_uuid, str):
        return get(thing_or_uuid)
    return thing_or_uuid


def remove(thing_or_uuid):
    """
    Method to delete a Thing from the mapping.

    Args:
        thing_or_uuid (str|Thing): Thing or UUID of Thing to be removed

    Returns:
        bool: True if the object was removed, False if the object did not exist in the mapping
    """
    _d_mapping = _get_mapping()
    thing_or_uuid = thing_or_uuid.get_uuid() if isinstance(thing_or_uuid, Thing) else thing_or_uuid
    if thing_or_uuid not in _d_mapping:
        return False

    _d_mapping[thing_or_uuid]._on_remove()
    del _d_mapping[thing_or_uuid]
    return True


def size():
    """
    return the number of Things in the mapping

    Returns:
        int: Number of Things in the mapping
    """
    return len(_get_mapping())


def list_entries(b_print=True):
    """
    This method builds a nicely formatted string to visualize the UUID and
    the `repr(Thing)` and optionally prints it as well.

    Args:
        b_print (bool): Whether to print this as well or not.

    Returns:
        str: The mapping in text form
    """
    s_line = f"+{45 * '-'}"
    s_prefix = f"| Codex:\n"
    s_content = ""
    for s_uuid, thing in _get_mapping().items():
        s_content += f"|    [{s_uuid}] " + repr(thing).replace("\n", f"\n|{(len(s_uuid)+7)*' '}") + "\n"

    s = s_line + "\n" + s_prefix + s_content + s_line
    if b_print:
        print(s)
    return s


if __name__ == "__main__":
    # create a few things
    thing1 = Thing()
    thing2 = Thing()
    thing3 = Thing()

    # print the Codex
    list_entries()

    # remove a few things
    remove(thing1)
    remove(thing3)

    # print the Codex again
    list_entries()
