# libs
Libraries I coded myself, for fun mostly

## 1) JSON.cpp
A small library that can be used to parse valid json string into a Node* object. 

### How to use:
```C++
#include<iostream>
#include<string>

#include "JSON.hpp"  
...
int main(){
	// parsing json
	json::Node* n = json::parse("{\"key\": \"value\", \"a\": 12}");
	
	// converting back to JSON string
	std::string jsonStr = n->toString();
	
	// retrieving values
	json::Object* o = (json::Object*)n;
	json::String* v = (json::String)o->get("key");
	std::string s = v->value;
	
	// json::Object node stores references to it's children in the
	// inherited 'children' member and int the key-value map
	for(int i=0;i<o->size();i++) Node* c = o->children[i];
	
	for(const auto& kvp : *o->map){
		std::string key = kvp.first;
		Node* c = kvp.second;
		...
	}
	
	// when you're done with it, just delete the parent node (will delete all it's children recursively).
	delete n;
	
	return 0;
}
...
```

### Lua support (lua headers must be included before the JSON.hpp file):
```C++
// include lua header/s here

#define JSON_LUA
#include "JSON.hpp"

int main(){
	lua_State* L = luaL_newstate();
	...
	
	json::Node* n = json::parse(<some_json_string>);
	n->toLuaStack(L);
	delete n;
	
	// now the Lua stack contains whatever data was in the JSON string.
	
	return 0;
}
```

