#include "astGenerator.h"
#include <memory>

#define SUBTYPE_MODE_SUBVARIABLE 0
#define SUBTYPE_MODE_SUBARGUMENT 1

template <class T> std::string CombineWhile(ASTParser::ASTPosition& position, const T& checkFunc, bool(*filterAllows)(Token& token)=&ASTParser::ASTPosition::FilterWhitespaceComments)
{
	std::string combiner;
	while(position.GetToken().TokenType != Token::Type::EndOfStream && checkFunc(position))
	{
		combiner += position.GetToken().TokenData;
		position.Increment();
	}
	return combiner;
}

template <class T> std::string CombineWhile_ScopeAware(ASTParser::ASTPosition& position, const T& checkFunc, bool(*filterAllows)(Token& token) = &ASTParser::ASTPosition::FilterWhitespaceComments)
{
	int count[4] = {0,0,0,0};

	std::string combiner;
	while(position.GetToken().TokenType != Token::Type::EndOfStream )
	{
		if(count[0] == 0 && count[1] == 0 && count[2] == 0 && count[3] == 0 && checkFunc(position) == false)
			break;
		if(position.GetToken().TokenType == Token::Type::LBrace)
			count[0]++;
		if(position.GetToken().TokenType == Token::Type::LBracket)
			count[1]++;
		if(position.GetToken().TokenType == Token::Type::LParen)
			count[2]++;
		if(position.GetToken().TokenType == Token::Type::LArrow)
			count[3]++;
		if(position.GetToken().TokenType == Token::Type::RBrace)
		{
			count[0]--;
			if(count[0] < 0)
				break;
				//throw std::runtime_error("too many right braces <}> found");
		}
		if(position.GetToken().TokenType == Token::Type::RBracket)
		{
			count[1]--;
			if(count[1] < 0)
				break;
				//throw std::runtime_error("too many right brackets <]> found");
		}
		if(position.GetToken().TokenType == Token::Type::RParen)
		{
			count[2]--;
			if(count[2] < 0)
				break;
				//throw std::runtime_error("too many right parentheses <)> found");
		}
		if(position.GetToken().TokenType == Token::Type::RArrow)
		{
			count[3]--;
			if(count[3] < 0)
				break;
				//throw std::runtime_error("too many right parentheses \">\" found");
		}

		combiner += position.GetToken().TokenData;
		position.Increment();
	}
	return combiner;
}

static std::string CombineTokens(ASTTokenSource* source, std::vector<ASTTokenIndex>& tokens, std::string joinSequence)
{
	std::string combiner;
	for(int i=0; i<static_cast<int>(tokens.size())-1; i++)
	{
		combiner += source->Tokens[tokens[i]].TokenData + joinSequence;
	}
	if(tokens.size() != 0)
		combiner += source->Tokens[tokens.back()].TokenData;

	return combiner;
}

ASTParser::ASTParser( Tokenizer& fromTokenizer )
{
	int lineNumber=1;
	Token token;
	while(token.TokenType != Token::Type::EndOfStream)
	{
		token = fromTokenizer.GetNextToken();
		token.TokenLine = lineNumber;
		if(token.TokenType == Token::Type::Newline)
		{
			lineNumber++;
		}
		else
		{
			
		}
		Tokens.push_back(token);
	}
}

bool ASTParser::Parse( ASTNode* parent, ASTPosition& position)
{
	while(ParseRoot(parent, position)) 
	{ 

	}

	return true;
}

bool ASTParser::ParseRoot( ASTNode* parent, ASTPosition& position )
{
	if(ParseEndOfStream(parent, position))
	{
		return false; // EOF
	}
	else if(ParseEnum(parent, position))
	{
		return true;
	}
	else if(ParseClass(parent, position))
	{
		return true;
	}
	else if (ParseDeclaration(1, parent, position))
	{
		return true;
	}
	else if (ParsePreprocessor(parent, position))
	{
		return true;
	}
	else if(ParseIgnored(parent, position))
	{
		return true;
	}


	return ParseUnknown(parent, position);
}

bool ASTParser::ParsePreprocessor(ASTNode* parent, ASTPosition& position)
{

	if (position.GetToken().TokenType == Token::Type::Hash)
	{
		std::vector<ASTTokenIndex> preprocessorTokens;

		while (true)
		{

			if (position.GetToken().TokenType == Token::Type::Newline || position.GetToken().TokenType == Token::Type::EndOfStream)
				break;

			preprocessorTokens.push_back(position.GetTokenIndex());
			position.Increment(1, ASTPosition::FilterNone);
		}

		printf(" * ignoring preprocessor directive: \"%s\"\n", CombineTokens(this, preprocessorTokens, "").c_str());
		return true;
	}

	return false;
}


bool ASTParser::ParseEnum( ASTNode* parent, ASTPosition& position )
{
	std::unique_ptr<ASTNode> subNode(new ASTNode());
	subNode->type = "ENUM";

	if(position.GetToken().TokenType != Token::Type::Enum)
		return false;

	position.Increment();

	if(position.GetToken().TokenType == Token::Type::Class)
	{
		// c++11 enum class
		subNode->type = "ENUM_CLASS";
		position.Increment();
	}

	if(position.GetToken().TokenType != Token::Type::Keyword)
		throw std::runtime_error("expected keyword after enum definition");

	subNode->data.push_back(position.GetToken().TokenData);

	if(position.GetNextToken().TokenType != Token::Type::LBrace)
		throw std::runtime_error("expected left brace after enum keyword");

	position.Increment();

	while(true)
	{
		if(ParseEnumDefinition(subNode.get(), position))
		{

		}
		else if(position.GetToken().TokenType == Token::Type::RBrace)
			break;
		else
			position.Increment();
	}

	if(position.GetNextToken().TokenType != Token::Type::Semicolon)
		throw std::runtime_error("expected semicolon after enum closing brace");
	
	position.Increment();

	// store subnode
	parent->children.push_back(subNode.release());
	return true;
}

bool ASTParser::ParseEnumDefinition( ASTNode* parent, ASTPosition& position )
{
	if(position.GetToken().TokenType != Token::Type::Keyword)
		return false;

	std::unique_ptr < ASTNode> subNode(new ASTNode());
	subNode->type = "ENUM_DEFINITION";

	// store token
	subNode->data.push_back(position.GetToken().TokenData);
	position.Increment();

	if(position.GetToken().TokenType == Token::Type::Equals)
	{
		position.Increment();

		// add the rest
		// add initialization clause
		std::unique_ptr < ASTNode> subSubNode(new ASTNode());
		subSubNode->type = "INIT";

		subSubNode->data.push_back(CombineWhile_ScopeAware(position, [] ( ASTPosition& position) { return position.GetToken().TokenType != Token::Type::Comma && position.GetToken().TokenType != Token::Type::RBrace && position.GetToken().TokenType != Token::Type::Semicolon; }, &ASTParser::ASTPosition::FilterComments));
		subNode->children.push_back(subSubNode.release());
	}
	// store subnode
	parent->children.push_back(subNode.release());
	return true;
}

bool ASTParser::ParseIgnored( ASTNode* parent, ASTPosition& position )
{
	auto& token = position.GetToken();
	if(token.TokenType == Token::Type::CommentMultiLine || token.TokenType == Token::Type::CommentSingleLine || token.TokenType == Token::Type::Whitespace || token.TokenType == Token::Type::Newline)
	{
		position.Increment();
		return true;
	}
	return false;
}

bool ASTParser::ParseUnknown( ASTNode* parent, ASTPosition& position )
{
	printf(" token: %d (type: %d, line: %d): %s\n", position.Position, position.GetToken().TokenType, position.GetToken().TokenLine, position.GetToken().TokenData.c_str());
	position.Increment();
	return true;
}

bool ASTParser::ParseEndOfStream( ASTNode* parent, ASTPosition& position )
{
	if(position.GetToken().TokenType == Token::Type::EndOfStream)
		return true;

	return false;
}

bool ASTParser::ParseClassInheritance(int &inheritancePublicPrivateProtected, ASTNode* parent, ASTPosition& cposition)
{
	ASTPosition position = cposition;
	if(position.GetToken().TokenType == Token::Type::Public)
	{
		inheritancePublicPrivateProtected = 0;
		position.Increment();
	}
	else if(position.GetToken().TokenType == Token::Type::Private)
	{
		inheritancePublicPrivateProtected = 1;
		position.Increment();
	}
	else if (position.GetToken().TokenType == Token::Type::Protected)
	{
		inheritancePublicPrivateProtected = 2;
		position.Increment();
	}
	
	if (position.GetToken().TokenType != Token::Type::Keyword)
		return false;
	
	ASTNode* subNode = new ASTNode();
	subNode->type = "INHERIT";
	if(inheritancePublicPrivateProtected == 0)
		subNode->data.push_back("public");
	else if(inheritancePublicPrivateProtected == 1)
		subNode->data.push_back("private");
	else if(inheritancePublicPrivateProtected == 2)
		subNode->data.push_back("protected");

	subNode->data.push_back(position.GetToken().TokenData);
	parent->children.push_back(subNode);
	position.Increment();
	cposition = position;
	return true;
}

bool ASTParser::ParseClass( ASTNode* parent, ASTPosition& position )
{
	if(position.GetToken().TokenType != Token::Type::Class)
		return false;

	int privatePublicProtected = 0;
	std::unique_ptr<ASTNode> subNode(new ASTNode());
	subNode->type = "CLASS";

	if(position.GetNextToken().TokenType != Token::Type::Keyword)
		throw std::runtime_error("expected class name after class definition");

	subNode->data.push_back(position.GetToken().TokenData);

	position.Increment();

	if(position.GetToken().TokenType == Token::Type::LBrace)
	{
		// brace - class definition starts now
	}
	else if(position.GetToken().TokenType == Token::Type::Colon)
	{
		position.Increment();

		// colon - inheritance
		int inheritancePublicPrivateProtected = 1;
		while(ParseClassInheritance(inheritancePublicPrivateProtected, subNode.get(), position)) 
		{ 
			if(position.GetToken().TokenType == Token::Type::Comma) 
			{ 
				position.Increment(); 
				continue;
			}
			else 
				break; 
		}
	}
	else
		throw std::runtime_error("expected left brace after class name");

	position.Increment();

	while(true)
	{
		if(ParsePrivatePublicProtected(privatePublicProtected, subNode.get(), position)) {}
		else if(ParseDeclaration(privatePublicProtected, subNode.get(), position)) {}
		else if (ParseClassConstructorDestructor(subNode.get(), position, privatePublicProtected)) {}
		//else if(ParseFunction(privatePublicProtected, subNode.get(), position)) {} // subclass
		else if(ParseClass(subNode.get(), position)) {} // subclass
		else if(ParseEnum(subNode.get(), position)) {} // sub enum
		else if(position.GetToken().TokenType == Token::Type::RBrace)
			break;
		else
			position.Increment();
	}

	if(position.GetNextToken().TokenType != Token::Type::Semicolon)
		throw std::runtime_error("expected semicolon after class closing brace");

	position.Increment();

	// store subnode
	parent->children.push_back(subNode.release());
	return true;
}

bool ASTParser::ParsePrivatePublicProtected(int& privatePublicProtected,  ASTNode* parent, ASTPosition& cposition )
{
	ASTPosition position = cposition;

	if(position.GetToken().TokenType == Token::Type::Private)
	{
		if(position.GetNextToken().TokenType == Token::Type::Colon)
		{
			privatePublicProtected = 0;
			position.Increment();
			cposition = position;
			return true;
		}
	}

	if(position.GetToken().TokenType == Token::Type::Public)
	{
		if(position.GetNextToken().TokenType == Token::Type::Colon)
		{
			privatePublicProtected = 1;
			position.Increment();
			cposition = position;
			return true;
		}
	}

	if(position.GetToken().TokenType == Token::Type::Protected)
	{
		if(position.GetNextToken().TokenType == Token::Type::Colon)
		{
			privatePublicProtected = 2;
			position.Increment();
			cposition = position;
			return true;
		}
	}

	return false;
}

enum class ASTFunctionType
{
	Regular,
	Constructor,
	Destructor,
	Operator
};


bool ASTParser::ParseConstructorInitializer(ASTNode* parent, ASTPosition& cposition)
{
	ASTPosition position = cposition;
	std::string name = "";
	
	// find keyword
	if (position.GetToken().TokenType != Token::Type::Keyword)
		return false;

	// store name and increment
	name = position.GetToken().TokenData;
	position.Increment();

	// find left parenthesis
	if (position.GetToken().TokenType != Token::Type::LParen)
		return false;

	std::vector<ASTTokenIndex> scopeTokens;
	if (ParseSpecificScopeInner(position, scopeTokens, Token::Type::LParen, Token::Type::RParen, &ASTPosition::FilterComments) == false)
		return false;

	position.Increment(); // skip rparen

	// everything checked out - add constructor initializer node
	std::unique_ptr<ASTNode> subNode = std::unique_ptr<ASTNode>(new ASTNode);
	subNode->type = "CONSTRUCTOR_INITIALIZER";
	subNode->data.push_back(name);
	subNode->data.push_back(CombineTokens(this, scopeTokens, ""));
	parent->children.push_back(subNode.release());

	// advance position to current and return success
	cposition = position;
	return true;
}


bool ASTParser::ParseSubType(int privatePublicProtected, std::unique_ptr<ASTNode>& varNode, ASTPosition &position, std::unique_ptr<ASTType> &varType, int mode)
{
	if (mode == SUBTYPE_MODE_SUBARGUMENT)
	{
		// parse base type
		if (ParseNTypeBase(position, varType.get()) == false)
			return false;
	}

	// TODO: ignore template definition for now - fix this
	std::vector<ASTTokenIndex> templateScopeTokens;
	if (ParseSpecificScopeInner(position, templateScopeTokens, Token::Type::LArrow, Token::Type::RArrow, &ASTPosition::FilterWhitespaceComments))
	{
		position.Increment();
	}

	// make preamble of variable
	if (privatePublicProtected == 0)
		varNode->data.push_back("private");
	if (privatePublicProtected == 1)
		varNode->data.push_back("public");
	if (privatePublicProtected == 2)
		varNode->data.push_back("protected");

	// parse pointers and references into type
	ParseNTypePointersAndReferences(position, varType.get());

	if (mode == SUBTYPE_MODE_SUBVARIABLE && position.GetToken().TokenType != Token::Type::Keyword && position.GetToken().TokenType != Token::Type::Operator) // subvariable must have identifier
		return false;

	// get variable name if available
	if (position.GetToken().TokenType == Token::Type::Keyword || position.GetToken().TokenType == Token::Type::Operator)
	{
		// store variable name
		bool isOperator = position.GetToken().TokenType == Token::Type::Operator;
		std::vector<ASTTokenIndex> nameTokens;
		nameTokens.push_back(position.GetTokenIndex());
		position.Increment();

		// parse namespaces
		while (position.GetToken().TokenType == Token::Type::Doublecolon)
		{
			nameTokens.push_back(position.GetTokenIndex()); // add doublecolon
			position.Increment();

			nameTokens.push_back(position.GetTokenIndex()); // add identifier
			if (position.GetToken().TokenType == Token::Type::Operator)
			{
				isOperator = true;
				position.Increment();
				break; // process operator later
			}
			if (position.GetToken().TokenType != Token::Type::Keyword)
				return false;
			position.Increment();
		}

		varNode->data.push_back(CombineTokens(this, nameTokens, ""));

		if (isOperator)
		{
			std::vector<ASTTokenIndex> operatorTokens;
			if (ParseFunctionOperatorType(varNode.get(), position, operatorTokens) == false)
				return false;

			varNode->data.push_back(CombineTokens(this, operatorTokens, ""));
		}
	}
	else
	{
		varNode->data.push_back("");
	}

	// parse array definitions
	ParseNTypeArrayDefinitions(position, varType.get());

	// assignment
	if (position.GetToken().TokenType == Token::Type::Equals)
	{
		position.Increment();

		// add initialization clause
		ASTNode* subSubNode = new ASTNode();
		subSubNode->type = "INIT";

		subSubNode->data.push_back(CombineWhile_ScopeAware(position, [](ASTPosition& position) { return position.GetToken().TokenType != Token::Type::Semicolon && position.GetToken().TokenType != Token::Type::Comma; }, &ASTParser::ASTPosition::FilterComments));
		varNode->children.push_back(subSubNode);
	}

	// TODO: templates
	if (templateScopeTokens.size() > 0)
		printf(" * ignoring template scope: %s\n", CombineTokens(this, templateScopeTokens, "").c_str());

	return true;
}

bool ASTParser::ParseFunctionArguments(ASTNode* varNode, int privatePublicProtected, ASTPosition &cposition)
{
	ASTPosition position = cposition;
	// find all function arguments
	while (true)
	{
		std::unique_ptr<ASTNode> argNode(new ASTNode());
		std::unique_ptr<ASTType> argType(new ASTType(this));
		if (ParseSubType(privatePublicProtected, argNode, position, argType, SUBTYPE_MODE_SUBARGUMENT) == false)
			break;

		argNode->type = "ARGUMENT";
		argNode->children.push_back(argType.release());
		varNode->children.push_back(argNode.release());

		if (position.GetToken().TokenType == Token::Type::Comma)
		{
			position.Increment();
			continue;
		}
		else if (position.GetToken().TokenType == Token::Type::RParen)
		{
			break;
		}
		else
			return false; // not actually a function definition, or one with an error.

	};

	position.Increment();
	cposition = position;
	return true;
}

bool ASTParser::ParseDeclaration( int privatePublicProtected, ASTNode* parent, ASTPosition& cposition )
{
	ASTPosition position = cposition;
	std::unique_ptr<ASTType> subTypeBase(new ASTType(this));

	// exclude operators when found
	if (position.GetToken().TokenType != Token::Type::Operator)
	{
		// parse type
		if (ParseNTypeBase(position, subTypeBase.get()) == false)
			return false;
	}


	// parse partial variables
	std::vector<std::unique_ptr<ASTNode>> subNodes;
	while(true) 
	{ 
		std::unique_ptr<ASTNode> varNode(new ASTNode()); 
		std::unique_ptr<ASTType> varType(new ASTType(*subTypeBase.get()));

		if (ParseSubType(privatePublicProtected, varNode, position, varType, SUBTYPE_MODE_SUBVARIABLE) == false)
			return false;

		if (position.GetToken().TokenType == Token::Type::LParen)
		{
			// function found.
			varNode->type = "FUNCTION";
			if (ParseFunctionRemainder(varNode.get(), position, privatePublicProtected) == false)
				return false;

			if (ParseFunctionFinalizer(varNode.get(), position, privatePublicProtected) == false)
				return false;

			
			varNode->children.insert(varNode->children.begin(), varType.release());
			subNodes.push_back(std::move(varNode));

			if (position.GetToken().TokenType == Token::Type::Comma)
			{
				// another function is defined
				position.Increment();
				continue;
			}


			break;
		}
		else
		{
			// should be a variable
			varNode->type = "VARIABLE";

			// check whether the variable is not invalid
			if (Tokens[varType->typeName[0]].TokenType == Token::Type::Void && varType->typePointers.size() == 0)
				throw std::runtime_error("Variables can not be of void type");
		}
		
		// check for more or terminator
		if(position.GetToken().TokenType == Token::Type::Comma)
		{
			// loop again, we should have another variable
			position.Increment();

			varNode->children.insert(varNode->children.begin(), varType.release());
			subNodes.push_back(std::move(varNode));
			continue;
		}
		else if(position.GetToken().TokenType == Token::Type::Semicolon)
		{
			// end of declaration
			position.Increment();

			varNode->children.insert(varNode->children.begin(), varType.release());
			subNodes.push_back(std::move(varNode));
			break; // end of statement - variable(s) parsed successfully
		}
		else
		{
			return false; // not a valid variable construct
		}
	}

	// everything went smoothly - store and return success
	cposition = position;
	for (auto& it : subNodes)
		parent->children.push_back(it.release());
	return true;
}

bool ASTParser::ParsePointerReferenceSymbol( ASTPosition &cposition, std::vector<Token> &pointerTokens, std::vector<Token> &pointerModifierTokens )
{
	bool isReference = false;
	ASTPosition position(cposition);
	Token ptrToken;

	if(position.GetToken() == Token::Type::Asterisk || position.GetToken() == Token::Type::Ampersand)
	{
		ptrToken = position.GetToken();
	}
	else
	{
		return false;
	}
	position.Increment();

	// check for pointer/reference modifiers
	while(position.GetToken() == Token::Type::Const || position.GetToken() == Token::Type::Volatile)
	{
		if(ptrToken == Token::Type::Ampersand)
			throw std::exception("modifiers (const/volatile) are not allowed on a reference");

		if(position.GetToken() == Token::Type::Const) // const applies to the thing to the left (so it could apply to the pointer)
			pointerModifierTokens.push_back(position.GetToken());
		if(position.GetToken() == Token::Type::Volatile) // volatile applies to the thing to the left (so it could apply to the pointer)
			pointerModifierTokens.push_back(position.GetToken());

		position.Increment();
	}

	// parse succeeded
	cposition = position;
	pointerTokens.push_back(ptrToken);
	return true;
}

bool ASTParser::ParseSpecificScopeInner(ASTPosition& cposition, std::vector<ASTTokenIndex> &insideBracketTokens, Token::Type tokenTypeL, Token::Type tokenTypeR, bool(*filterAllows)(Token& token) /*= &ASTPosition::FilterWhitespaceComments*/)
{
	ASTPosition position(cposition);

	if (position.GetToken().TokenType != tokenTypeL)
		return false;
	position.Increment(1, filterAllows);

	while (position.GetToken().TokenType != tokenTypeR)
	{
		if (position.GetToken().TokenType == Token::Type::EndOfStream)
			throw new std::runtime_error("error while parsing scope - end of file found before closing token was found");

		if (position.GetToken().TokenType == tokenTypeL)
		{
			// add LBracket
			insideBracketTokens.push_back(position.GetTokenIndex());

			// add inner
			std::vector<ASTTokenIndex> inside;
			if (ParseSpecificScopeInner(position, inside, tokenTypeL, tokenTypeR, filterAllows) == false)
				return false;
			insideBracketTokens.insert(insideBracketTokens.end(), inside.begin(), inside.end());

			// add RBracket
			insideBracketTokens.push_back(position.GetTokenIndex());
			position.Increment(1, filterAllows);
		}
		else
		{
			insideBracketTokens.push_back(position.GetTokenIndex());
			position.Increment(1, filterAllows);
		}
	}

	cposition = position;
	return true;
}

bool ASTParser::ParseFunctionOperatorType(ASTNode* parent, ASTPosition& cposition, std::vector<ASTTokenIndex>& ctokens)
{
	std::vector<ASTTokenIndex> tokens;
	ASTPosition position = cposition;

	// parse operator () (special case)
	if (position.GetToken().TokenType == Token::Type::LParen)
	{
		tokens.push_back(position.GetTokenIndex());
		position.Increment();
		
		if (position.GetToken().TokenType == Token::Type::RParen)
		{
			tokens.push_back(position.GetTokenIndex());
			position.Increment();
		}
		else
			return false;
	}
	else
	{
		// parse other operators
		while (position.GetToken().TokenType != Token::Type::EndOfStream)
		{
			if (position.GetToken().TokenType == Token::Type::LParen)
				break; // start of function arguments found
			else if (position.GetToken().TokenType == Token::Type::Semicolon)
				throw new std::runtime_error("semicolon found while parsing operator arguments - we must have gone too far. invalid operator?");
			else
			{
				tokens.push_back(position.GetTokenIndex());
				position.Increment();
			}
		}

	}

	cposition = position;
	ctokens.swap(tokens);
	return true;

}

bool ASTParser::ParseModifierToken(ASTPosition& position, std::vector<ASTTokenIndex>& modifierTokens)
{
	if (position.GetToken().TokenType == Token::Type::Const)
		modifierTokens.push_back(position.GetTokenIndex());
	else if (position.GetToken().TokenType == Token::Type::Inline)
		modifierTokens.push_back(position.GetTokenIndex());
	else if (position.GetToken().TokenType == Token::Type::Extern)
		modifierTokens.push_back(position.GetTokenIndex());
	else if (position.GetToken().TokenType == Token::Type::Virtual)
		modifierTokens.push_back(position.GetTokenIndex());
	else if (position.GetToken().TokenType == Token::Type::Volatile)
		modifierTokens.push_back(position.GetTokenIndex());
	else if (position.GetToken().TokenType == Token::Type::Unsigned)
		modifierTokens.push_back(position.GetTokenIndex());
	else if (position.GetToken().TokenType == Token::Type::Static)
		modifierTokens.push_back(position.GetTokenIndex());
	else if (position.GetToken().TokenType == Token::Type::Mutable)
		modifierTokens.push_back(position.GetTokenIndex());
	else
		return false;
	position.Increment();
	return true;
}
bool ASTParser::ParseNTypeBase(ASTPosition &position, ASTType* typeNode)
{
	std::vector<ASTTokenIndex>& typeTokens = typeNode->typeName;
	std::vector<ASTTokenIndex>& modifierTokens = typeNode->typeModifiers;
	std::vector<ASTTokenIndex>& namespaceTokens = typeNode->typeNamespaces;
	int typeWordIndex = -1;
	while (true)
	{
		if (position.GetToken().TokenType == Token::Type::Keyword)
		{
			if (typeWordIndex == -1)
			{
				typeTokens.push_back(position.GetTokenIndex());
				typeWordIndex = typeTokens.size() - 1;
			}
			else
				break; // this must be the variable name (two keywords not allowed in a type)
		}
		else if (position.GetToken().TokenType == Token::Type::BuiltinType || position.GetToken().TokenType == Token::Type::Void)
		{
			if (typeWordIndex == -1)
			{
				typeTokens.push_back(position.GetTokenIndex());
				typeWordIndex = typeTokens.size() - 1;
			}
			else
				return false; // invalid - type has already been set to a keyword
		}
		else if (ParseModifierToken(position, modifierTokens))
			continue;
		else if (position.GetToken().TokenType == Token::Type::Doublecolon)
		{
			if (typeWordIndex != -1)
			{
				if (Tokens[typeTokens[typeWordIndex]].TokenType == Token::Type::Keyword)
				{
					namespaceTokens.push_back(typeTokens[typeWordIndex]);
					typeTokens.clear();
					typeWordIndex = -1; // namespace found - reset type word index
				}
				else if (Tokens[typeTokens[typeWordIndex]].TokenType == Token::Type::BuiltinType)
					throw std::runtime_error("cannot namespace a built-in type");
				else if (Tokens[typeTokens[typeWordIndex]].TokenType == Token::Type::Void)
					throw std::runtime_error("cannot namespace a built-in type");
			}
		}
		else
			return typeWordIndex != -1;
		position.Increment();
	}

	return true;
}

bool ASTParser::ParseNTypeSinglePointersAndReferences(ASTPosition &cposition, ASTType* typeNode)
{
	bool isReference = false;
	ASTPosition position(cposition);
	ASTPointerType ptrData;

	Token ptrToken;

	if (position.GetToken() == Token::Type::Asterisk || position.GetToken() == Token::Type::Ampersand)
	{
		if (position.GetToken() == Token::Type::Asterisk)
			ptrData.pointerType = ASTPointerType::Type::Pointer;
		else if (position.GetToken() == Token::Type::Ampersand)
			ptrData.pointerType = ASTPointerType::Type::Reference;
		ptrToken = position.GetToken();
	}
	else
	{
		return false;
	}
	position.Increment();

	// check for pointer/reference modifiers
	while (position.GetToken() == Token::Type::Const || position.GetToken() == Token::Type::Volatile)
	{
		if (ptrToken == Token::Type::Ampersand)
			throw std::exception("modifiers (const/volatile) are not allowed on a reference");

		if (position.GetToken() == Token::Type::Const) // const applies to the thing to the left (so it could apply to the pointer)
			ptrData.pointerModifiers.push_back(position.GetToken());
		if (position.GetToken() == Token::Type::Volatile) // volatile applies to the thing to the left (so it could apply to the pointer)
			ptrData.pointerModifiers.push_back(position.GetToken());

		position.Increment();
	}

	// parse succeeded
	cposition = position;
	ptrData.pointerToken = ptrToken;
	typeNode->typePointers.push_back(ptrData);
	return true;
}

bool ASTParser::ParseNTypePointersAndReferences(ASTPosition &position, ASTType* typeNode)
{
	bool res = false;
	while (ParseNTypeSinglePointersAndReferences(position, typeNode))
	{
		res |= true;
	}
	return res;
}

bool ASTParser::ParseNTypeArrayDefinitions(ASTPosition &position, ASTType* typeNode)
{
	// parse arrays
	bool hasArray = false;

	std::vector<ASTTokenIndex> arrayTokens;
	while (ParseSpecificScopeInner(position, arrayTokens, Token::Type::LBracket, Token::Type::RBracket, &ASTParser::ASTPosition::FilterComments))
	{
		// mark that an array has been found
		hasArray = true;

		// add array tokens to array by pushing a new vector and swapping.
		typeNode->typeArrayTokens.push_back(std::vector<ASTTokenIndex>());
		typeNode->typeArrayTokens.back().swap(arrayTokens);

		// go to next token (advance past RBracket )
		position.Increment(); 
	}

	return hasArray;
}

bool ASTParser::ParseFunctionRemainder(ASTNode* varNode, ASTPosition &cposition, int privatePublicProtected)
{
	bool success = false;
	ASTPosition position = cposition;
	// we should start at LParen
	if (position.GetToken().TokenType != Token::Type::LParen)
		return false;
	
	position.Increment(); // advance past LParen

	// parse function arguments
	if (position.GetToken().TokenType != Token::Type::RParen)
	{
		// we are not. try parsing subvariables
		if (ParseFunctionArguments(varNode, privatePublicProtected, position) == false)
			return false;
	}
	else
	{
		// advance past rparen
		position.Increment();
	}

	cposition = position;
	return true;
}

bool ASTParser::ParseFunctionFinalizer(ASTNode* varNode, ASTPosition& position, int privatePublicProtected)
{
	// parse remainder
	bool pureVirtual = false;
	bool isConst = false;
	if (position.GetToken().TokenType == Token::Type::Const)
	{
		isConst = true;
		position.Increment();
	}

	if (position.GetToken().TokenType == Token::Type::Equals)
	{
		// pure virtual
		position.Increment();

		if (position.GetToken().TokenType != Token::Type::Number || position.GetToken().TokenData != "0")
			return false;
		position.Increment();

		if (position.GetToken().TokenType != Token::Type::Semicolon && position.GetToken().TokenType != Token::Type::Comma)
			return false;
		position.Increment();

		pureVirtual = true;
	}

	// store const/virtual
	if (isConst || pureVirtual)
	{
		ASTNode* subSubNode = new ASTNode();
		subSubNode->type = "FUNCTION_PROPERTIES";
		if (isConst)
			subSubNode->data.push_back("const");
		if (pureVirtual)
			subSubNode->data.push_back("pure");
		varNode->children.push_back(subSubNode);
	}

	if (position.GetToken().TokenType == Token::Type::Semicolon)
	{
		// function definition
		position.Increment();
	}
	else if (position.GetToken().TokenType == Token::Type::LBrace)
	{
		// function declaration
		std::vector<ASTTokenIndex> scopeTokens;
		if (ParseSpecificScopeInner(position, scopeTokens, Token::Type::LBrace, Token::Type::RBrace, &ASTParser::ASTPosition::FilterComments) == false)
			return false;

		position.Increment(); // skip rbrace

		ASTNode* subSubNode = new ASTNode();
		subSubNode->type = "DEFINITION";
		subSubNode->data.push_back(CombineTokens(this, scopeTokens, ""));
		varNode->children.push_back(subSubNode);
	}
	else if (position.GetToken().TokenType == Token::Type::Comma)
	{

	}
	else
		return false;



	return true;
}

bool ASTParser::ParseClassConstructorDestructor(ASTNode* parent, ASTPosition &cposition, int privatePublicProtected)
{
	ASTPosition position = cposition;
	std::vector<ASTTokenIndex> modifierTokens;
	
	// parse pre-keyword modifier tokens
	while (ParseModifierToken(position, modifierTokens)) {}

	// is this a destructor?
	bool destructor = false;
	if (position.GetToken().TokenType == Token::Type::Tilde)
	{
		destructor = true;
		position.Increment();
	}

	if (position.GetToken().TokenType != Token::Type::Keyword)
		return false;

	if (parent->type != "CLASS")
		return false;

	if (position.GetToken().TokenData != parent->data[0])
		return false;

	position.Increment();

	// parse post-keyword modifier tokens
	while (ParseModifierToken(position, modifierTokens)) {}

	std::unique_ptr<ASTNode> varNode(new ASTNode());
	if (ParseFunctionRemainder(varNode.get(), position, privatePublicProtected) == false)
		return false;

	// parse constructor initialization if applicable
	if (position.GetToken().TokenType == Token::Type::Colon)
	{
		if (destructor)
			throw new std::runtime_error("destructor with initializer list is prohibited");
		position.Increment();
		while (ParseConstructorInitializer(varNode.get(), position)) { if(position.GetToken().TokenType == Token::Type::Comma) position.Increment(); }
	}

	// parse final function stage
	if (ParseFunctionFinalizer(varNode.get(), position, privatePublicProtected) == false)
		return false;

	// everything checked out

	// generate type
	std::unique_ptr<ASTType> varType(new ASTType(this));
	varType->typeModifiers.swap(modifierTokens);
	varNode->children.insert(varNode->children.begin(), varType.release());

	if (destructor == false)
		varNode->type = "CONSTRUCTOR";
	else
		varNode->type = "DESTRUCTOR";

	// add visibility
	if (privatePublicProtected == 0)
		varNode->data.push_back("private");
	else if (privatePublicProtected == 1)
		varNode->data.push_back("public");
	else if (privatePublicProtected == 2)
		varNode->data.push_back("protected");

	parent->children.push_back(varNode.release());

	cposition = position;
	return true;
}

const std::string& ASTType::GetType() const
{
	static std::string t = "TYPE"; return t;
}


std::string ASTPointerType::ToString()
{
	std::string ret;
	if (pointerType == Type::Pointer)
		ret = "*";
	else 
		ret = "&";
	for (auto& it : pointerModifiers)
		ret += " " + it.TokenData;
	return ret;
}


std::string ASTType::ToString()
{
	std::string ret;
	for (auto& it : typeModifiers)
		ret += tokenSource->Tokens[it].TokenData + " ";
	for (auto& it : typeNamespaces)
		ret += tokenSource->Tokens[it].TokenData + "::";
	for (auto& it : typeName)
		ret += tokenSource->Tokens[it].TokenData + " ";
	for (auto& it : typePointers)
		ret += it.ToString() + " ";
	for (auto& it : typeArrayTokens)
	{
		ret += "[";
		for (auto& it2 : it)
			ret += tokenSource->Tokens[it2].TokenData;
		ret += "]";
	}
		

	return ret;

}
const std::vector<std::string>& ASTType::GetData()
{
	data.clear();
	data.push_back(ToString());
	return data;
}


#pragma region ASTPosition
void ASTParser::ASTPosition::Increment(int count/*=1*/, bool(*filterAllows)(Token& token)/*=0*/)
{
	for (int i = 0; i < count; i++)
	{
		// check whether we are at the end of the token stream
		if (Position + 1 >= Parser.Tokens.size())
			break;

		++Position;

		if (filterAllows(GetToken()) == false)
		{
			// this token is filtered, go to the next
			i--;
		}
	}
}

ASTParser::ASTPosition::ASTPosition( ASTParser& parser ) : Parser(parser)
{
	Position = 0;
}

Token& ASTParser::ASTPosition::GetNextToken()
{
	Increment();

	return Parser.Tokens[Position];
}

Token& ASTParser::ASTPosition::GetToken()
{
	return Parser.Tokens[Position];
}

#pragma endregion
