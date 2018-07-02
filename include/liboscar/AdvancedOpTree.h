#ifndef LIBOSCAR_ADVANCED_OP_TREE_H
#define LIBOSCAR_ADVANCED_OP_TREE_H
#include <string>
#include <vector>
#include <sserialize/strings/stringfunctions.h>
#include <sserialize/utility/assert.h>

/** The AdvancedOpTree supports the following query language:
  *
  *
  * Q := FM_CONVERSION Q | DILATION Q | COMPASS Q | Q BETWEEN_OP Q | Q BINARY_OP Q
  * Q := (Q) | Q Q
  * Q := ITEM | GEO_RECT | GEO_PATH | REGION | CELL
  * FM_CONVERSION := %
  * DILATION_OP := CELL_DILATION | REGION_DILATOIN
  * CELL_DILATION := %NUMBER%
  * REGION_DILATION := %#NUMBER%
  * COMPASS_OP := :^ | :v | :> | :< | :north-of | :east-of | :south-of | :west-of
  * USER_FRIENDLY := :in | :near | in | near
  * RELEVANT_ELEMENT_OP := *
  * BETWEEN_OP := <-> | :between
  * BINARY_OP := - | + | INTERSECTION | ^ |
  * INTERSECTION := ' ' | / | , | .
  * ITEM := $item:<itemId>
  * GEO_RECT := $geo:<rect-defintion>
  * POINT := $point:radius,lat,lon
  * POLYGON := $poly:[lat,lon]
  * GEO_PATH := $path:radius,[lat,lon]
  * REGION := $region:<storeId>
  * REGION_EXCLUSIVE_CELLS := $rec:<storeId>
  * QUERY_EXCLUSIVE_CELLS := $qec:<minDirectParents>:<maxDirectParents>
  * CONSTRAINED_REGION_EXCLUSIVE_CELLS := $crec:<storeId>,<rect-definition>
  * CELL := $cell:<cellId>|$cell:lat,lon
  * CELLS := $cells:<cellids>
  * TRIANGLE := $triangle:<triangleId>
  * TRIANGLES := $triangles:<triangleId>
  */
namespace liboscar {
namespace detail {
namespace AdvancedOpTree {

struct Node {
	enum Type : int { UNARY_OP, BINARY_OP, LEAF};
	enum OpType : int {
		FM_CONVERSION_OP, CELL_DILATION_OP, REGION_DILATION_OP, COMPASS_OP, RELEVANT_ELEMENT_OP,
		IN_OP, NEAR_OP,
		SET_OP, BETWEEN_OP,
		QUERY_EXCLUSIVE_CELLS,
		RECT, POLYGON, PATH, POINT,
		REGION, REGION_EXCLUSIVE_CELLS, CONSTRAINED_REGION_EXCLUSIVE_CELLS,
		CELL, CELLS,
		TRIANGLE, TRIANGLES,
		STRING, ITEM, STRING_ITEM, STRING_REGION
	};
	int baseType;
	int subType;
	std::string value;
	std::vector<Node*> children;
	Node() {}
	Node(int baseType, int subType, const std::string & value) : baseType(baseType), subType(subType), value(value) {}
	~Node() {
		for(Node* & n : children) {
			delete n;
			n = 0;
		}
	}
};

namespace parser {

struct Token {
	enum Type : int {
		//store chars in the lower 8 bits
		ENDOFFILE = 0,
		INVALID_TOKEN = 258,
		INVALID_CHAR,
		FM_CONVERSION_OP,
		CELL_DILATION_OP,
		REGION_DILATION_OP,
		COMPASS_OP,
		RELEVANT_ELEMENT_OP,
		BETWEEN_OP,
		IN_OP,
		NEAR_OP,
		SET_OP,
		GEO_RECT,
		GEO_POLYGON,
		GEO_PATH,
		GEO_POINT,
		REGION,
		REGION_EXCLUSIVE_CELLS,
		QUERY_EXCLUSIVE_CELLS,
		CONSTRAINED_REGION_EXCLUSIVE_CELLS,
		CELL,
		CELLS,
		TRIANGLE,
		TRIANGLES,
		STRING,
		ITEM,
		STRING_ITEM,
		STRING_REGION
		
	};
	int type;
	std::string value;
	Token() : type(INVALID_TOKEN) {}
	Token(int type) : type(type) {}
};

class Tokenizer {
public:
	struct State {
		std::string::const_iterator it;
		std::string::const_iterator end;
	};
private:
	//reserved for the future in case string hinting is needed, should get optimized away
	struct StringHinter {
		inline bool operator()(const std::string::const_iterator & /*begin*/, const std::string::const_iterator & /*end*/) const { return false; }
	};
public:
	Tokenizer();
	Tokenizer(std::string::const_iterator begin, std::string::const_iterator end); 
	Tokenizer(const State & state);
	Token next();
private:
	std::string readString();
private:
	static bool isWhiteSpace(char c);
	static bool isOperator(char c);
	static bool isScope(char c);
private:
	State m_state;
	StringHinter * m_strHinter;
};

class Parser {
public:
	Parser();
	Node * parse(const std::string & str);
private:
	Token peek();
	bool eat(liboscar::detail::AdvancedOpTree::parser::Token::Type t);
	bool pop();
private:
	Node* parseUnaryOps();
	Node* parseSingleQ();
	Node* parseQ();
private:
	std::string m_str;
	Token m_prevToken;
	Token m_lastToken;
	Tokenizer m_tokenizer;
};

}}}//end namespace detail::AdvancedOpTree::parser

class AdvancedOpTree {
public:
	typedef detail::AdvancedOpTree::Node Node;
public:
public:
	AdvancedOpTree();
	AdvancedOpTree(const AdvancedOpTree &) = delete;
	virtual ~AdvancedOpTree();
	AdvancedOpTree & operator=(const AdvancedOpTree &) = delete;
	virtual void parse(const std::string & str);
public:
	///get the root node, do not alter it!
	const Node * root() const { return m_root; }
	Node * root() { return m_root; }
private:
	Node * m_root;
};

}//end namespace

#endif
