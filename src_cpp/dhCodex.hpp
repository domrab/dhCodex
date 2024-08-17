/* dhCodex - C++ - v1.0.0

[LICENSE BELOW]

The idea behind the codex is to hand over ownership of custom objects (Things)
to one central authority (the Codex) and use strings containing the UUIDs to
retrieve raw pointers.
Since ownership is managed by the Codex, you are not allowed to delete any raw
pointers to Things.
The Codex maps std::string UUIDs to std::unique_ptr<Thing>.

Taking a parent-child relationship as an example, instead of the parent owning
the child, both are owned by the Codex. To make the relationship, the parent
would have a member <std::vector<std::string>> children containing the UUIDs of
all of its children. At the same time, the child can have a member
<std::string> parent containing the UUID of the parent.
Both can have methods <std::vector<Thing*>> get_children() and <Thing* get_parent()>
respectively, retrieving a raw pointer to the respective objects. This method
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
*/

#pragma once

#ifndef DH_CODEX_IMPLEMENTATION
#define DH_CODEX_IMPLEMENTATION

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <mutex>
#include <type_traits>


namespace dh {
namespace codex {
    // this will be defined later
    class Thing;

// internal stuff, no need to expose that to users
namespace {
#ifdef __APPLE__
#error __new_uuid() not implemented for apple yet!
    // apple implementation of _new_uuid()
    const std::string _new_uuid() {}

#elif defined(_WIN32)
    #pragma comment(lib, "rpcrt4.lib")
    #include <Rpc.h>

    /**
     * @brief Generates a new UUID
     * 
     * The UUID is used as key into the codex to retrieve the object. Its also supposed
     * to be used to create 'soft' relationships.
     * 
     * @return UUID
    */
    const std::string _new_uuid()
    {
        UUID uuid;
        long status = UuidCreate(&uuid);
        if (status != 0) {
            std::cout << "Unexpected error retrieveing UUID! Error code: " << status << std::endl;
            return "";
        }
        unsigned char* str;
        status = UuidToStringA(&uuid, &str);
        if (status != 0) {
            std::cout << "Unexpected error converting UUID to string! Error code: " << status << std::endl;
            return "";
        }
        std::string s((char*)str);
        RpcStringFreeA(&str);
        return s;
    };

#elif defined(__linux__)
    #include <uuid/uuid.h>
    
    /**
     * @brief Generates a new UUID
     * 
     * The UUID is used as key into the codex to retrieve the object. Its also supposed
     * to be used to create 'soft' relationships.
     * 
     * @return UUID
    */
    const std::string _new_uuid() {
        // Create a UUID object
        uuid_t uuid;
        uuid_generate(uuid);

        char uuid_str[37]; // UUID string representation (36 characters + null terminator)
        uuid_unparse(uuid, uuid_str);

        return std::string(uuid_str);
    }

#endif

    /**
     * @brief Getter for the mutex
     * 
     * A static local variable holds the mutex which is used in all the threadsafe methods
    */
    inline std::mutex* _get_mutex(){
        static std::mutex mutex;
        return &mutex;
    }

    /**
     * @brief Getter for the codex (global)
     * 
     * A static local variable holds the mapping. Any modifications to the codex is
     * global and therefore has to be managed in a threadsafe manner.
    */
    inline std::map<const std::string, std::unique_ptr<Thing>>* _get_mapping() {
        // Maps the UUID to std::unique_ptr{Thing}
        static std::map<const std::string, std::unique_ptr<Thing>> _mapping;
        return &_mapping;
    }

    /**
     * @brief Find one Thing with the given UUID
     *
     * This is an internal method, the public interface is get()
     * This method is not thread safe and you could potentially run into
     * issues if the codex is modified while this method is running on
     * another thread.
     *
     * @tparam T The type of Thing to be retrieved
     *
     * @param uuid The UUID to query
     *
     * @return A pointer to the Thing if found, otherwise nullptr.
    */
    template <typename T = Thing>
    T* _find_one_by_uuid__unsafe(const std::string& uuid) {
        auto _mapping = _get_mapping();
        auto it = _mapping->find(uuid);
        return (it != _mapping->end()) ? dynamic_cast<T*>(it->second.get()) : nullptr;
    };

    /**
     * @brief Find one Thing with the given UUID
     *
     * This is an internal method, the public interface is get()
     * This method is threadsafe.
     *
     * @tparam T The type of Thing to be retrieved
     *
     * @param uuid The UUID to query
     *
     * @return A pointer to the Thing if found, otherwise nullptr.
    */
    template <typename T = Thing>
    T* _find_one_by_uuid(const std::string& uuid) {
        std::lock_guard<std::mutex> lock{ *_get_mutex() };
        return _find_one_by_uuid__unsafe<T>(uuid);
    }
};

    enum class Status {
        SUCCESS = 0,
        FAILURE
    };

    /**
     * @brief Adds an object of type T to the Codex
     *
     * This function takes a unique_ptr to an instance of Thing (or a subclass) and
     * adds it to the Codex. The Codex assumes ownership going forward
     * This method is not thread safe and you could potentially run into
     * issues if the codex is modified while this method is running on
     * another thread.
     *
     * @tparam T The type of the object to be added. Must be a subclass of Thing
     *
     * @param ptr The unique ptr owning the object to be added
     *
     * @return A raw pointer to the object
    */
    template<typename T>
    T* add__unsafe(std::unique_ptr<T> ptr) {
        static_assert(std::is_base_of<Thing, T>::value, "T must inherit from Thing");
        auto uuid = ptr->get_uuid();
        (*_get_mapping())[uuid] = std::move(ptr);
        return dynamic_cast<T*>(_get_mapping()->at(uuid).get());
    }

    /**
     * @brief Adds an object of type T to the Codex
     *
     * This function takes a unique_ptr to an instance of Thing (or a subclass) and
     * adds it to the Codex. The Codex assumes ownership going forward
     * This method is threadsafe.
     *
     * @tparam T The type of the object to be added. Must be a subclass of Thing
     *
     * @param ptr The unique ptr owning the object to be added
     *
     * @return A raw pointer to the object
    */
    template<typename T>
    T* add(std::unique_ptr<T> ptr) {
        std::lock_guard<std::mutex> lock{ *_get_mutex() };
        return add__unsafe<T>(std::move(ptr));
    }

    // Base object for Codex
    class Thing {
    private:
        const std::string _uuid;

    public:
        /**
         * @brief create() becomes the defacto 'constructor' to be used to create Thing objects
         *
         * While this could be implemented however one likes, the important part is that after creating
         * a unique ptr, it needs to get passed to add() for the codex to take ownership. This cant be
         * part of the constructor as there is no clean way to turn `this` into a unique_ptr.
         * Personally I like `create()` as it can mimic the signature of the actual constructor and just
         * pass along the arguments.
        */
        static Thing* create() {
            return add(std::make_unique<Thing>());
        }

        Thing() : _uuid(_new_uuid()) {};

        /**
         * @brief The destructor gets called when a Thing is removed from the Codex
         * 
         * Removing should always happen through `remove()` as this locks a mutex to
         * prevent the Codex from getting out of sync with other threads. 
         * Since `remove()` calls the destructor already, calling `remove()` from within
         * the destructor would try to lock the mutex again. Therefore, when removing
         * dependencies from a node (eg children), you should call `remove__unsafe()`
         * instead of `remove()` since you are already in a 'safe' environment.
         * If you create new threads in the destructor, you are on your own... Thats way
         * beyond my knowledge. Try to avoid it if possible :)
        */
        virtual ~Thing() {};

        /**
         * @brief uuid getter
         *
         * @return uuid
        */
        const std::string get_uuid() const { return this->_uuid; }

        /**
         * @brief Generates a simple string representation of the object
         *
         * @return UUID
        */
        virtual const std::string get_repr() const {
            return "<'" + std::string(typeid(*this).name()) + "' object at [" + this->get_uuid() + "]>";
        };
    };

    /**
     * @brief Find one Thing with the given UUID and print an error if no object can be found
     *
     * This method is not thread safe and you could potentially run into
     * issues if the codex is modified while this method is running on
     * another thread.
     *
     * @tparam T The type of Thing to be retrieved
     *
     * @param uuid The UUID to query
     *
     * @return A pointer to the Thing if found, otherwise nullptr.
    */
    template <typename T = Thing>
    T* get__unsafe(const std::string& uuid) {
        static_assert(std::is_base_of<Thing, T>::value, "<T> must be a subclass of Thing");
        auto result = _find_one_by_uuid__unsafe(uuid);
        return (result != nullptr) ? dynamic_cast<T*>(result) : nullptr;
    };

    /**
     * @brief Find one Thing with the given UUID and print an error if no object can be found
     *
     * This method is threadsafe.
     *
     * @tparam T The type of Thing to be retrieved
     *
     * @param uuid The UUID to query
     *
     * @return A pointer to the Thing if found, otherwise nullptr.
    */
    template <typename T = Thing>
    T* get(const std::string& uuid) {
        std::lock_guard<std::mutex> lock{ *_get_mutex() };
        return get_unsafe<T>(uuid);
    }

    /**
     * @brief Remove a Thing from the Codex via UUID
     *
     * This method is not thread safe and you could potentially run into
     * issues if the codex is modified while this method is running on
     * another thread.
     * When removing nodes, you should generally use `remove()`. This locks
     * a mutex to make a thread safe environment. Therefore, anything in
     * the destructor can assumed to be threadsafe. To avoid locking the
     * same mutex again, use `remove__unsafe()` in the destructor to delete
     * dependencies.
     *
     * @param uuid The UUID of the object to remove
     *
     * @return Status::SUCCESS or Status::Failure (if UUID not in Codex)
    */
    Status remove__unsafe(const std::string& uuid) {
        Thing* entry = get__unsafe<Thing>(uuid);
        if (entry == nullptr) {
            return Status::FAILURE;
        }
        _get_mapping()->erase(uuid);
        return Status::SUCCESS;
    };

    /**
     * @brief Remove a Thing from the Codex via UUID
     *
     * This method is threadsafe.
     *
     * @param uuid The UUID of the object to remove
     *
     * @return Status::SUCCESS or Status::Failure (if UUID not in Codex)
    */
    Status remove(const std::string& uuid) {
        std::lock_guard<std::mutex> lock{ *_get_mutex() };
        return remove__unsafe(uuid);
    }

    /**
     * @brief Remove a Thing form the Codex via pointer
     *
     * This method is threadsafe.
     *
     * @param ptr A ptr to the Thing to remove
     *
     * @return Status::SUCCESS or Status::Failure (if UUID not in Codex)
    */
    Status remove(Thing* ptr) {
        std::lock_guard<std::mutex> lock{ *_get_mutex() };
        return remove__unsafe(ptr->get_uuid());
    }

    /**
     * @brief Return the number of Things in the Codex
     *
     * This method is not thread safe and you could potentially run into
     * issues if the codex is modified while this method is running on
     * another thread.
     *
     * @return Number of Things in the Codex
    */
    const size_t size__unsafe() {
        return _get_mapping()->size();
    }

    /**
     * @brief Return the number of Things in the Codex
     *
     * This method is threadsafe.
     *
     * @return Number of Things in the Codex
    */
    const size_t size() {
        std::lock_guard<std::mutex> lock{ *_get_mutex() };
        return size__unsafe();
    }


    /**
     * @brief Creates a table as string with the Codex's elements
     *
     * This method is not thread safe and you could potentially run into
     * issues if the codex is modified while this method is running on
     * another thread.
     *
     * @parm print_to_stdout If true, sends string to stdout
     *
     * @return Same string that gets printed
    */
    std::string list_entries__unsafe(const bool& print = true) {
        auto _mapping = _get_mapping();
        std::string line = "+---------------------------------------------";
        std::string prefix = "\n| Codex:\n";
        std::string content = "";
        for (auto it = _mapping->begin(); it != _mapping->end(); it++) {
            std::string repr = it->second->get_repr();
            std::string repr_offset = "";
            std::string indent = "|       ";
            for(int _=0; _<it->first.size(); _++) indent += " ";

            for (int idx=0; idx<repr.size(); idx++) {
                repr_offset += repr.at(idx);
                if (repr.at(idx) == '\n') repr_offset += indent;
            }

            content += "|    [" + it->first + "] " + repr_offset + "\n";
        }
        if (print) { std::cout << line << prefix << content << line << std::endl; }
        return line + prefix + content + line + "\n";
    }

    /**
     * @brief Creates a table as string with the Codex's elements
     *
     * This method is threadsafe.
     *
     * @parm print_to_stdout If true, sends string to stdout
     *
     * @return Same string that gets printed
    */
    std::string list_entries(const bool& print = true) {
        std::lock_guard<std::mutex> lock{ *_get_mutex() };
        return list_entries__unsafe(print);
    }
};
};

#endif // !DH_CODEX_IMPLEMENTATION
