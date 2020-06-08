#include <liboscar/AdvancedOpTree.h>

namespace liboscar {
namespace detail {
namespace AdvancedOpTree {
namespace parser {

Tokenizer::Tokenizer() : m_strHinter(0) {}

Tokenizer::Tokenizer(const Tokenizer::State& state) :
m_state(state),
m_strHinter(new StringHinter())
{}

Tokenizer::Tokenizer(std::string::const_iterator begin, std::string::const_iterator end) {
	m_state.it = begin;
	m_state.end = end;
}

bool Tokenizer::isScope(char c) {
	return (c == '(' || c == ')');
}


bool Tokenizer::isWhiteSpace(char c) {
	return (c == ' ' || c == '\t');
}

bool Tokenizer::isOperator(char c) {
	return (c == '+' || c == '-' || c == '/' || c == '^' || c == '%' || c == ':' || c == '<' || c == '*');
}

std::string Tokenizer::readString() {
	std::string tokenString;
	int lastValidStrSize = -1; //one passed the end == size of the valid string
	std::string::const_iterator lastValidStrIt = m_state.it;
	if (*m_state.it == '?') {
		tokenString += '?';
		++m_state.it;
		lastValidStrIt = m_state.it;
		lastValidStrSize = (int) tokenString.size();
	}
	if (*m_state.it == '"') {
		tokenString += *m_state.it;
		++m_state.it;
		while(m_state.it != m_state.end) {
			if (*m_state.it == '\\') {
				++m_state.it;
				if (m_state.it != m_state.end) {
					tokenString += *m_state.it;
					++m_state.it;
				}
				else {
					break;
				}
			}
			else if (*m_state.it == '"') {
				tokenString += *m_state.it;
				++m_state.it;
				break;
			}
			else {
				tokenString += *m_state.it;
				++m_state.it;
			}
		}
		lastValidStrIt = m_state.it;
		lastValidStrSize = (int) tokenString.size();
		if (m_state.it != m_state.end && *m_state.it == '?') {
			tokenString += *m_state.it;
			++m_state.it;
			lastValidStrIt = m_state.it;
			lastValidStrSize = (int) tokenString.size();
		}
	}
	else {
		if (lastValidStrSize > 0) { //we've read a '?'
			lastValidStrIt = m_state.it-1;
			lastValidStrSize = -1;
		}
		while (m_state.it != m_state.end) {
			if (*m_state.it == '\\') {
				++m_state.it;
				if (m_state.it != m_state.end) {
					tokenString += *m_state.it;
					++m_state.it;
				}
				else
					break;
			}
			else if (*m_state.it == ' ' || *m_state.it == '.' || *m_state.it == ',') {
				tokenString += *m_state.it;
				if (m_strHinter && m_strHinter->operator()(tokenString.cbegin(), tokenString.cend())) {
					if (tokenString.size() > 1 && tokenString.at(tokenString.size()-2) != ' ') {
						lastValidStrSize = (int) (tokenString.size()-1);
						lastValidStrIt = m_state.it;
					}
					++m_state.it;
				}
				else {
					tokenString.pop_back();
					break;
				}
			}
			else if (isScope(*m_state.it)) {
				//we've read a string with spaces, check if all up to here is also part of it
				if (lastValidStrSize >= 0 && tokenString.size() && tokenString.back() != ' ' && m_strHinter->operator()(tokenString.cbegin(), tokenString.cend())) {
					lastValidStrSize = (int) tokenString.size();
					lastValidStrIt = m_state.it;
				}
				break;
			}
			else if (isOperator(*m_state.it)) {
				if (tokenString.size() && tokenString.back() == ' ') {
					break;
				}
				else {
					tokenString += *m_state.it;
					++m_state.it;
				}
			}
			else {
				tokenString += *m_state.it;
				++m_state.it;
			}
		}
		if (lastValidStrSize < 0 || (m_state.it == m_state.end && m_strHinter->operator()(tokenString.cbegin(), tokenString.cend()))) {
			lastValidStrSize = (int) tokenString.size();
			lastValidStrIt = m_state.it;
		}
		else if (lastValidStrSize > 0) {
			m_state.it = lastValidStrIt;
		}
	}
	tokenString.resize(lastValidStrSize);
	return tokenString;
}

//TODO:use ragel to parse? yes, use ragel since this gets more and more complex
Token Tokenizer::next() {
	if (m_state.it == m_state.end) {
		return Token(Token::ENDOFFILE);
	}
	Token t;
	while (m_state.it != m_state.end) {
		switch (*m_state.it) {
		case '%':
		{
			t.type = Token::FM_CONVERSION_OP;
			t.value += *m_state.it;
			++m_state.it;
			//check for modifiers
			if (m_state.it != m_state.end) {
				if (*m_state.it == '#') {
					t.type = Token::REGION_DILATION_BY_CELL_COVERAGE_OP;
					++m_state.it;
				}
				else if (*m_state.it == '!') {
					t.type = Token::REGION_DILATION_BY_ITEM_COVERAGE_OP;
					++m_state.it;
				}
				else if('0' <= *m_state.it && '9' >= *m_state.it) {
					t.type = Token::CELL_DILATION_OP;
				}
			}
			//check if theres a number afterwards, because then this is dilation operation
			if (m_state.it != m_state.end && ('0' <= *m_state.it && '9' >= *m_state.it)) {
				bool ok = false;
				for(auto it(m_state.it); it != m_state.end; ++it) {
					if ('0' > *it || '9' < *it) {
						if (*it == '%') {
							ok = true;
							t.value.assign(m_state.it, it);
							++it;
							m_state.it = it;
						}
						break;
					}
				}
				if (!ok) {
					t.type = Token::FM_CONVERSION_OP;
				}
			}
			return t;
		}
		case '+':
		case '-':
		case '/':
		case '^':
			t.type = Token::SET_OP;
			t.value += *m_state.it;
			++m_state.it;
			return t;
		case ' ': //ignore whitespace
		case '\t':
		case ',':
		case '.':
			++m_state.it;
			break;
		case '(':
		case ')':
			t.type = *m_state.it;
			t.value += *m_state.it;
			++m_state.it;
			return t;
		case '$': //parse a region/cell/geo/path query
		{
			//read until the first occurence of :
			std::string tmp;
			bool bracedParameterList = false;
			for(++m_state.it; m_state.it != m_state.end;) {
				if (*m_state.it == ':') {
					++m_state.it;
					break;
				}
				else if (*m_state.it == '(') {
					++m_state.it;
					bracedParameterList = true;
					break;
				}
				else {
					tmp += *m_state.it;
					++m_state.it;
				}
			}
			bool opSeparates = false;
			if (tmp == "region") {
				t.type = Token::REGION;
				opSeparates = true;
			}
			else if (tmp == "rec") {
				t.type = Token::REGION_EXCLUSIVE_CELLS;
				opSeparates = true;
			}
			else if (tmp == "qec") {
				t.type = Token::QUERY_EXCLUSIVE_CELLS;
			}
			else if (tmp == "cell") {
				t.type = Token::CELL;
				opSeparates = true;
			}
			else if (tmp == "cells") {
				t.type = Token::CELLS;
			}
			else if (tmp == "triangle") {
				t.type = Token::TRIANGLE;
			}
			else if (tmp == "triangles") {
				t.type = Token::TRIANGLES;
			}
			else if (tmp  == "item") {
				t.type = Token::ITEM;
				opSeparates = true;
			}
			else if (tmp == "geo") {
				t.type = Token::GEO_RECT;
			}
			else if (tmp == "poly") {
				t.type = Token::GEO_POLYGON;
			}
			else if (tmp == "path") {
				t.type = Token::GEO_PATH;
			}
			else if (tmp == "point") {
				t.type = Token::GEO_POINT;
			}
			if (bracedParameterList) {
				std::size_t braceCount = 1;
				for(; m_state.it != m_state.end && braceCount; ++m_state.it) {
					if (*m_state.it == '(') {
						braceCount += 1;
					}
					else if (*m_state.it == ')') {
						braceCount -= 1;
					}
					t.value += *m_state.it;
				}
				if (!braceCount && t.value.size()) {
					SSERIALIZE_CHEAP_ASSERT_EQUAL(')', t.value.back());
					t.value.pop_back();
				}
			}
			else {
				for(; m_state.it != m_state.end;) {
					if (isWhiteSpace(*m_state.it) || (opSeparates && isOperator(*m_state.it)) || isScope(*m_state.it)) {
						break;
					}
					else {
						t.value += *m_state.it;
						++m_state.it;
					}
				}
			}
			return t;
			break;
		}
		case ':':
		{
			t.type = Token::COMPASS_OP;
			++m_state.it;
			for(auto it(m_state.it); it != m_state.end; ++it) {
				if (*it == ' ' || *it == '\t' || *it == '(') {
					t.value.assign(m_state.it, it);
					m_state.it = it;
					break;
				}
			}
			if (t.value == "between") {
				t.type = Token::BETWEEN_OP;
				t.value = "<->";
			}
			else if (t.value == "in") {
				t.type = Token::IN_OP;
				t.value = "in";
			}
			else if (t.value == "near") {
				t.type = Token::NEAR_OP;
				t.value = "near";
			}
			return t;
		}
		case '*':
		{
			t.type = Token::RELEVANT_ELEMENT_OP;
			++m_state.it;
			t.value = "*";
			return t;
		}
		case '<':
		{
			t.type = Token::BETWEEN_OP;
			const char * cmp = "<->";
			auto it(m_state.it);
			for(int i(0); it != m_state.end && i < 3 && *it == *cmp; ++it, ++cmp, ++i) {}
			t.value.assign(m_state.it, it);
			m_state.it = it;
			return t;
		}
		case '#':
		{
			t.type = Token::STRING_REGION;
			++m_state.it;
			t.value = readString();
			return t;
		}
		case '!':
		{
			t.type = Token::STRING_ITEM;
			++m_state.it;
			t.value = readString();
			return t;
		}
		default: //read as normal string
		{
			t.type = Token::STRING;
			t.value = readString();
			if (t.value == "between") {
				t.type = Token::BETWEEN_OP;
				t.value = "<->";
			}
			else if (t.value == "in") {
				t.type = Token::IN_OP;
				t.value = "in";
			}
			else if (t.value == "near") {
				t.type = Token::NEAR_OP;
				t.value = "near";
			}
			return t;
		}
		};
	}
	if (m_state.it == m_state.end) {
		t.type = Token::ENDOFFILE;
	}
	return t;
}

bool Parser::pop() {
	if (-1 == m_lastToken.type) {
		throw std::runtime_error("pop() without peek()");
	}
	m_prevToken = m_lastToken;
	// stop at EOF, otherwise consume it
	if (Token::ENDOFFILE != m_lastToken.type) {
		m_lastToken.type = -1;
	}
	return true;
}

//dont turn return type to reference or const reference since this function may be called while tehre ist still a reference on the return value
Token Parser::peek() {
	if (-1 == m_lastToken.type) {
		m_lastToken = m_tokenizer.next();
	}
	return m_lastToken;
}

bool Parser::eat(Token::Type t) {
	if (t != peek().type) {
		return false;
	}
	pop();
	return true;
}

detail::AdvancedOpTree::Node* Parser::parseUnaryOps() {
	Token t = peek();
	int nst = 0;
	switch (t.type) {
	case Token::CELL_DILATION_OP:
		nst = Node::CELL_DILATION_OP;
		break;
	case Token::IN_OP:
		nst = Node::IN_OP;
		break;
	case Token::NEAR_OP:
		nst = Node::NEAR_OP;
		break;
	case Token::REGION_DILATION_BY_CELL_COVERAGE_OP:
		nst = Node::REGION_DILATION_BY_CELL_COVERAGE_OP;
		break;
	case Token::REGION_DILATION_BY_ITEM_COVERAGE_OP:
		nst = Node::REGION_DILATION_BY_ITEM_COVERAGE_OP;
		break;
	case Token::FM_CONVERSION_OP:
		nst = Node::FM_CONVERSION_OP;
		break;
	case Token::COMPASS_OP:
		nst = Node::COMPASS_OP;
		break;
	case Token::RELEVANT_ELEMENT_OP:
		nst = Node::RELEVANT_ELEMENT_OP;
		break;
	case Token::QUERY_EXCLUSIVE_CELLS:
		nst = Node::QUERY_EXCLUSIVE_CELLS;
		break;
	default:
		break;
	}
	
	switch (t.type) {
	case Token::FM_CONVERSION_OP:
	case Token::CELL_DILATION_OP:
	case Token::NEAR_OP:
	case Token::IN_OP:
	case Token::REGION_DILATION_BY_CELL_COVERAGE_OP:
	case Token::REGION_DILATION_BY_ITEM_COVERAGE_OP:
	case Token::COMPASS_OP:
	case Token::RELEVANT_ELEMENT_OP:
	case Token::QUERY_EXCLUSIVE_CELLS:
	{
		pop();
		Node * unaryOpNode = new Node();
		unaryOpNode->baseType = Node::UNARY_OP;
		unaryOpNode->subType = nst;
		unaryOpNode->value = t.value;
		Node* cn = parseSingleQ();
		if (cn) {
			unaryOpNode->children.push_back(cn);
			return unaryOpNode;
		}
		else { //something went wrong, skip this op
			return 0;
		}
		break;
	}
	default://hand back
		return 0;
	};
	SSERIALIZE_CHEAP_ASSERT(false);
	return 0;
}

//parses a Single query like STRING, REGION, CELL, GEO_RECT, GEO_PATH
//calls parseQ() on opening a new SCOPE
detail::AdvancedOpTree::Node* Parser::parseSingleQ() {
	Token t = peek();
	switch (t.type) {
	case '(': //opens a new query
	{
		pop();
		Node * tmp = parseQ();
		eat((Token::Type)')');
		return tmp;
	}
	case Token::QUERY_EXCLUSIVE_CELLS:
	case Token::FM_CONVERSION_OP:
	case Token::CELL_DILATION_OP:
	case Token::REGION_DILATION_BY_CELL_COVERAGE_OP:
	case Token::REGION_DILATION_BY_ITEM_COVERAGE_OP:
	case Token::COMPASS_OP:
	case Token::RELEVANT_ELEMENT_OP:
	{
		return parseUnaryOps();
	}
	case Token::CELL:
	{
		pop();
		return new Node(Node::LEAF, Node::CELL, t.value);
	}
	case Token::CELLS:
	{
		pop();
		return new Node(Node::LEAF, Node::CELLS, t.value);
	}
	case Token::TRIANGLE:
	{
		pop();
		return new Node(Node::LEAF, Node::TRIANGLE, t.value);
	}
	case Token::TRIANGLES:
	{
		pop();
		return new Node(Node::LEAF, Node::TRIANGLES, t.value);
	}
	case Token::REGION:
	{
		pop();
		return new Node(Node::LEAF, Node::REGION, t.value);
		break;
	}
	case Token::REGION_EXCLUSIVE_CELLS:
	{
		pop();
		return new Node(Node::LEAF, Node::REGION_EXCLUSIVE_CELLS, t.value);
		break;
	}
	case Token::GEO_PATH:
	{
		pop();
		return new Node(Node::LEAF, Node::PATH, t.value);
		break;
	}
	case Token::GEO_POINT:
	{
		pop();
		return new Node(Node::LEAF, Node::POINT, t.value);
		break;
	}
	case Token::GEO_RECT:
	{
		pop();
		return new Node(Node::LEAF, Node::RECT, t.value);
		break;
	}
	case Token::GEO_POLYGON:
	{
		pop();
		return new Node(Node::LEAF, Node::POLYGON, t.value);
		break;
	}
	case Token::STRING:
	{
		pop();
		return new Node(Node::LEAF, Node::STRING, t.value);
		break;
	}
	case Token::STRING_ITEM:
	{
		pop();
		return new Node(Node::LEAF, Node::STRING_ITEM, t.value);
		break;
	}
	case Token::STRING_REGION:
	{
		pop();
		return new Node(Node::LEAF, Node::STRING_REGION, t.value);
		break;
	}
	case Token::ITEM:
	{
		pop();
		return new Node(Node::LEAF, Node::ITEM, t.value);
		break;
	}
	case Token::ENDOFFILE:
	default: //hand back to caller, somethings wrong
		return 0;
	};
}


//(((())) ggh)
//Q ++ Q
detail::AdvancedOpTree::Node* Parser::parseQ() {
	Node * n = 0;
	for(;;) {
		Token t = peek();
		Node * curTokenNode = 0;
		switch (t.type) {
		case ')': //leave scope, caller removes closing brace
			//check if there's an unfinished operation, if there is ditch it
			if (n && n->baseType == Node::BINARY_OP && n->children.size() == 1) {
				Node* tmp = n;
				n = tmp->children.front();
				tmp->children.clear();
				delete tmp;
			}
			return n;
			break;
		case Token::FM_CONVERSION_OP:
		case Token::IN_OP:
		case Token::NEAR_OP:
		case Token::CELL_DILATION_OP:
		case Token::REGION_DILATION_BY_CELL_COVERAGE_OP:
		case Token::REGION_DILATION_BY_ITEM_COVERAGE_OP:
		case Token::COMPASS_OP:
		case Token::RELEVANT_ELEMENT_OP:
		case Token::QUERY_EXCLUSIVE_CELLS:
		{
			curTokenNode = parseUnaryOps();
			if (!curTokenNode) { //something went wrong, skip this op
				continue;
			}
			break;
		}
		case Token::SET_OP:
		case Token::BETWEEN_OP:
		{
			pop();
			 //we need to have a valid child, otherwise this operation is bogus (i.e. Q ++ Q)
			if (n && !(n->baseType == Node::BINARY_OP && n->children.size() < 2)) {
				Node * opNode = new Node();
				opNode->baseType = Node::BINARY_OP;
				opNode->subType = (t.type == Token::SET_OP ? Node::SET_OP : Node::BETWEEN_OP);
				opNode->value = t.value;
				opNode->children.push_back(n);
				n = 0;
				//get the other child in the next round
				curTokenNode = opNode;
			}
			else { //skip this op
				continue;
			}
			break;
		}
		case Token::ENDOFFILE:
			return n;
			break;
		case Token::INVALID_TOKEN:
		case Token::INVALID_CHAR:
			pop();
			break;
		default:
			curTokenNode = parseSingleQ();
			break;
		};
		if (!curTokenNode) {
			continue;
		}
		if (n) {
			if (n->baseType == Node::BINARY_OP && n->children.size() < 2) {
				n->children.push_back(curTokenNode);
			}
			else {//implicit intersection
				Node * opNode = new Node(Node::BINARY_OP, Node::SET_OP, " ");
				opNode->children.push_back(n);
				opNode->children.push_back(curTokenNode);
				n = opNode;
			}
		}
		else {
			n = curTokenNode;
		}
	}
	//this should never happen
	SSERIALIZE_CHEAP_ASSERT(false);
	return n;
}

Parser::Parser() {
	m_prevToken.type = -1;
	m_lastToken.type = -1;
}

detail::AdvancedOpTree::Node* Parser::parse(const std::string & str) {
	m_str.clear();
	//sanitize string, add as many openeing braces at the beginning as neccarray or as manny closing braces at the end
	{
		int obCount = 0;
		for(char c : str) {
			if (c == '(') {
				++obCount;
				m_str += c;
			}
			else if (c == ')') {
				if (obCount > 0) {
					m_str += c;
					--obCount;
				}
				//else not enough opening braces, so skip this one
			}
			else {
				m_str += c;
			}
		}
		//add remaining closing braces to get a well-formed query
		for(; obCount > 0;) {
			m_str += ')';
			--obCount;
		}
	}
	m_tokenizer  = Tokenizer(m_str.begin(), m_str.end());
	m_lastToken.type = -1;
	m_prevToken.type = -1;
	Node* n = parseQ();
	return n;
}

}}}//end namespace detail::AdvancedOpTree::parser

AdvancedOpTree::AdvancedOpTree() :
m_root(0)
{}

AdvancedOpTree::~AdvancedOpTree() {
	if (m_root) {
		delete m_root;
	}
}

void AdvancedOpTree::parse(const std::string& str) {
	if (m_root) {
		delete m_root;
		m_root = 0;
	}
	detail::AdvancedOpTree::parser::Parser p;
	m_root = p.parse(str);
}

}//end namespace
