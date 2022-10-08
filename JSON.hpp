#pragma once

#include<iostream>
#include<string>
#include<vector>
#include<unordered_map>

/*
	Don't use the functions that start with _ because this library it's not intended to be used that way
	but nobody can stop you, so have at it.

	If you wish to push json::Node to a lua stack, declare JSON_LUA preprocessor before including this file
*/
namespace json {
	// Proto
	class Number;
	class String;
	class Object;
	class Array;
	class Boolean;

	#pragma region DataTypes
	enum Type { boolean, number, string, array, object, null };

	
	int _nodes_in_memory = 0;

	class Node {
		public:
		Type type;
		Node* parent;
		std::vector<Node*> children;

		Node(Type type) : type(type) {
			parent = NULL;
			_nodes_in_memory++;
		};

		~Node() {
			/*Every children will also trigger deletion of their children and so on*/
			while (children.size() > 0) {
				Node* n = children[children.size() - 1];
				delete n;
				children.pop_back();
			}

			_nodes_in_memory--;
		}
		/// <summary>
		/// Returns the JSON string equivalent of this node (Includes all children)
		/// </summary>
		virtual std::string toString() = 0;

		#ifdef JSON_LUA
		/// <summary>
		/// Pushes this to stack
		/// </summary>
		virtual void toLuaStack(lua_State* L) = 0;
		#endif
	};

	class Boolean : public Node {
		public:
		bool value;
		Boolean(bool v) : Node(Type::boolean), value(v){}

		std::string toString() { return value ? "true" : "false"; }

		#ifdef JSON_LUA
		void toLuaStack(lua_State* L) {
			lua_pushboolean(L, value);
		}
		#endif
	};

	class Null : public Node {
		public:
		Null() : Node(Type::null){}

		std::string toString() { return "null"; }

		#ifdef JSON_LUA
		void toLuaStack(lua_State* L) {
			lua_pushnil(L);
		}
		#endif
	};

	class Number : public Node {
		public:
		double value;
		Number(double val) : Node(Type::number), value(val) {}

		operator auto() const { return value; }

		std::string toString() { return std::to_string(value); }

		#ifdef JSON_LUA
		void toLuaStack(lua_State* L) {
			lua_pushnumber(L, value);
		}
		#endif

	};

	class String : public Node {
		public:
		std::string value;
		String(std::string val) : Node(Type::string), value(val) {}

		std::string toString() { return "\"" + value + "\""; }

		#ifdef JSON_LUA
		void toLuaStack(lua_State* L) {
			lua_pushstring(L, value.c_str());
		}
		#endif
	};

	class Object : public Node {
		public:
		Object() : Node(Type::object) { map = new std::unordered_map<std::string, Node*>(); }
		std::unordered_map<std::string, Node*>* map;

		int size() { return (int)map->size(); }
		void set(std::string key, Node* val) { (*map)[key] = val; }
		void set(String* s, Node* v) { set(s->value, v); }
		Node* get(std::string key) {
			if (map->count(key) > 0)
				return (*map)[key];
			return NULL;
		}
		void remove(std::string key) {
			if (map->count(key) == 0) return;
			Node* n = (*map)[key];
			
			auto it = remove_if(children.begin(), children.end(), [n](Node* n2) { return n2 == n; });
			delete n;
		}

		std::string toString() {
			std::string s = "{";
			for (const auto& a : *map) {
				if (s.size() > 1)
					s += ",";
				s += "\"" + a.first + "\":";
				s += a.second->toString();
			}
			s += "}";
			return s;
		}

		#ifdef JSON_LUA
		void toLuaStack(lua_State* L) {
			lua_newtable(L);
			int idx = lua_gettop(L);

			for (const auto& a : *map) {
				a.second->toLuaStack(L);
				lua_setfield(L, idx, a.first.c_str());
			}
		}
		#endif

		~Object() {
			for (auto itr = map->begin(); itr != map->end(); itr++) {
				Node* n = itr->second;
				delete n;
			}
			delete map;
		}
	};

	class Array : public Node {
		public:
		Array() : Node(Type::array) { }

		int size() { return (int)children.size(); }

		Node* operator [] (int index) const { return children[index]; }

		std::string toString() {
			std::string s = "[";
			for (const auto& n : children) {
				if (s.size() > 1) s += ",";
				s += n->toString();
			}
			s += "]";
			return s;
		}

		#ifdef JSON_LUA
		void toLuaStack(lua_State* L) {
			lua_newtable(L);
			int idx = lua_gettop(L);

			for (int i = 0; i < children.size(); i++) {
				children[i]->toLuaStack(L);
				std::string iStr = std::to_string(i);
				lua_rawseti(L, idx, i);
			}
		}
		#endif

		~Array() {
			for (int i = 0; i < children.size(); i++) {
				Node* n = children[i];
				delete n;
			}
		}
	};
	#pragma endregion

	#pragma region Proto
	std::exception _unexpectedEOF();
	#pragma endregion

	#pragma region _internal
	std::string _j;
	int _i;

	bool _iis() { _i++; if (_i >= _j.size()) { _i = (int)_j.size() - 1; return true; } return false; }
	void _ii() { _i++; if (_i >= _j.size()) throw _unexpectedEOF(); }
	char _c() { return _j[_i]; }
	bool _w() { char c = _c(); return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
	char _c(int i) { if (i + _i >= _j.size()) throw _unexpectedEOF();  return _j[_i + i]; }

	#pragma region Exceptions
	std::exception _unexpectedCharErr() {
		std::string err = "Unexpected character '";
		err.push_back(_c());
		err += "' at position " + std::to_string(_i);
		return std::exception(err.c_str());
	}
	std::exception _unexpectedEOF() {
		std::string err = "Unexpected end of file (";
		err += "at position ";
		err += std::to_string(_i);
		err += " in json string)";
		return std::exception(err.c_str());
	}
	#pragma endregion

	bool _eq(std::string s) {
		for (int i = 0; i < s.size(); i++)
			if (_c(i) != s[i])
				return false;
		return true;
	}

	void _skipWhiteSpaces(bool throwErr = true) {
		while (_w()) {
			if (!throwErr)
				if (_iis())
					return;
			_ii();
		}
	}


	void _hPrt(Node* parent, Node* child) {
		if (parent == NULL)
			return;
	
		parent->children.push_back(child);
		child->parent = parent;
	}

	#pragma region _parsing

	String* _parseString(Node* parent) {
		std::string val = "";
		char c = _c();
		
		_ii();
		while (_c() != c || _c(-1) == '\\') {
			val.push_back(_c());
			_ii();
		}
		_ii();

		String* s = new String(val);
		_hPrt(parent, s);
		return s;
	}

	Number* _parseNumber(Node* parent) {
		std::string str;
		while (_c() >= '0' && _c() <= '9' || _c() == '.' || _c() == '-') {
			str.push_back(_c());
			_ii();
		}

		char* end;
		double v = std::strtod(str.c_str(), &end);
		
		if (*end != '\0')
			throw "Invalid number in json at position " + _i;

		Number* n = new Number(v);
		_hPrt(parent, n);
		return n;
	}

	Boolean* _parseBoolean(Node* parent) {
		Boolean* b;

		if (_eq("true")) {
			b = new Boolean(true);
			_i += 4;
		}
		else if (_eq("false")) {
			b = new Boolean(false);
			_i += 5;
		}
		else throw _unexpectedCharErr();

		_hPrt(parent, b);
		return b;
	}

	Null* _parseNull(Node* parent) {
		if (!_eq("null"))
			throw _unexpectedCharErr();
		_i += 4;
		
		Null* n = new Null();
		_hPrt(parent, n);
		return n;
	}

	Object* _parseObject(Node* parent);
	Array* _parseArray(Node* parent);
	Node* _parse(Node* parent) {
		Node* n = NULL;

		_skipWhiteSpaces();

		char c = _c();
		switch (c) {
			case 'n':
				n = _parseNull(parent);
				break;

			case 't':
			case 'f':
				n = _parseBoolean(parent);
				break;

			case '-':
			case '.':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				n = _parseNumber(parent);
				break;

			case '"':
				n = _parseString(parent);
				break;

			case '{':
				n = _parseObject(parent);
				break;

			case '[':
				n = _parseArray(parent);
				break;

			default:
				throw _unexpectedCharErr();
				break;
		}

		return n;
	}

	Object* _parseObject(Node* parent) {
		Object* o = new Object();
		_hPrt(parent, o);

		_ii(); // skip {
		while (_c() != '}') {
			_skipWhiteSpaces(); // to next char that should be a '"'

			if (_c() != '"') throw _unexpectedCharErr();

			String* key = _parseString(NULL);

			_skipWhiteSpaces(); // to next char that should be a ':'

			if (_c() != ':') throw _unexpectedCharErr();
			
			_ii(); // jump to next char after ':'

			Node* val = _parse(o);

			_skipWhiteSpaces();

			if (_c() != ',' && _c() != '}') throw _unexpectedCharErr();
			if (_c() == ',') _ii();

			o->set(key, val);
			delete key;
		}
		_iis(); // json can end here

		return o;
	}

	Array* _parseArray(Node* parent) {
		Array* a = new Array();
		_hPrt(parent, a);

		_ii();
		while (_c() != ']') {
			_skipWhiteSpaces();
			Node* n = _parse(a);
			_skipWhiteSpaces();

			if (_c() != ',' && _c() != ']') throw _unexpectedCharErr();
			if (_c() == ',') _ii();
		}
		_iis(); // EOF can end in an array end

		return a;
	}
	#pragma endregion

	#pragma endregion

	Node* parse(std::string s) {
		_j = s;
		_i = 0;
		Node* n = NULL;
		try {
			n = _parse(NULL);
		}
		catch (std::exception e) {
			std::cerr << e.what();
		};

		return n;
	}
}