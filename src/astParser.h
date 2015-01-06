#pragma once

#include <vector>
#include "tokenizer.h"
#include <memory>
#include "ast.h"

struct ASTDeclarationParsingOptions
{
	explicit ASTDeclarationParsingOptions(bool _AllowCtor = false, bool _AllowBitfield = false, bool _RequireIdentifier = false) { AllowCtor = _AllowCtor; AllowBitfield = _AllowBitfield; RequireIdentifier = _RequireIdentifier; }
	bool AllowCtor = false;
	bool AllowBitfield = false;
	bool RequireIdentifier = false;
};

class ASTParser: public ASTTokenSource
{
public:

	class ASTPosition
	{
	public:
		ASTPosition(ASTParser& parser);
		ASTPosition& operator = (const ASTPosition& o) { this->Position = o.Position; return *this; }

		static bool FilterWhitespaceComments(Token& token) { if (token.TokenType == Token::Type::Newline || token.TokenType == Token::Type::Whitespace || token.TokenType == Token::Type::CommentMultiLine || token.TokenType == Token::Type::CommentSingleLine) return false; return true; }
		static bool FilterComments(Token& token) { if (token.TokenType == Token::Type::CommentMultiLine || token.TokenType == Token::Type::CommentSingleLine) return false; return true; }
		static bool FilterNone(Token& token) { return true; }

		void Increment(int count=1, bool (*filterAllows)(Token& token)=&FilterWhitespaceComments);
		
		Token& GetToken();
		Token& GetNextToken();
		ASTTokenIndex GetTokenIndex() { return Position; }

		ASTParser& Parser;
		ASTTokenIndex Position;
		operator ASTTokenIndex() { return Position; }
	};

	ASTParser() {}
	ASTParser(Tokenizer& fromTokenizer);
	
	bool Verbose = false;
	bool IsUTF8 = false;

	ASTNode ForwardAnnotationStack;

	bool Parse(ASTNode* parent, ASTPosition& position);
protected:
	bool ParseRootParticle(ASTNode* parent, ASTPosition& position);
	void ParseBOM(ASTPosition &position);

	bool ParseTemplate( ASTNode* parent, ASTPosition& position);
	bool ParseNamespace(ASTNode* parent, ASTPosition& position);
	bool ParseUsing(ASTNode* parent, ASTPosition& cposition);
	bool ParseFriend(ASTNode* parent, ASTPosition& cposition);
	bool ParseTypedef(ASTNode* parent, ASTPosition& cposition);
	bool ParseEnum(ASTNode* parent, ASTPosition& position);
	bool ParseEnumDefinition(ASTNode* parent, ASTPosition& position);
	bool ParseClass(ASTNode* parent, ASTPosition& position);

	int ParseClassParticle(int privatePublicProtected, ASTPosition &position, ASTDataNode*& currentScope, std::unique_ptr<ASTDataNode> &subNode, ASTNode* parent);

	bool ParseClassInheritance(int &inheritancePublicPrivateProtected, ASTNode* parent, ASTPosition& position);
	bool ParsePrivatePublicProtected(int& privatePublicProtected, ASTPosition& cposition );
	bool ParsePreprocessor(ASTNode* parent, ASTPosition& position);
	bool ParseConstructorInitializer(ASTType* parent, ASTPosition& cposition);
	bool ParseOperatorType(ASTNode* parent, ASTPosition& cposition, std::vector<ASTTokenIndex>& ctokens);
	bool ParseDeclaration(ASTNode* parent, ASTPosition& cposition, ASTDeclarationParsingOptions opts);
	bool _ParseDeclaration_HeadSubs(ASTPosition &position, ASTNode* parent, std::unique_ptr<ASTType> &headType, size_t &lastSubID, ASTDeclarationParsingOptions opts);

	bool ParseDeclarationHead(ASTNode* parent, ASTPosition& cposition, ASTType* type, ASTDeclarationParsingOptions opts);
	bool ParseDeclarationSub(ASTNode* parent, ASTPosition& cposition, ASTType* type, ASTType* headType, ASTDeclarationParsingOptions opts);

	bool ParseConstructorArguments(ASTNode* argNode, ASTPosition &position);

	bool ParseModifierToken(ASTPosition& position, std::vector<std::pair<ASTTokenIndex, ASTTokenIndex>>& modifierTokens);
	bool ParseArgumentAttribute(ASTPosition &position, std::pair<ASTTokenIndex, ASTTokenIndex>& outTokenStream);

	bool ParseClassConstructorDestructor(ASTNode* parent, ASTPosition &position);
	bool ParsePointerReferenceSymbol(ASTPosition &position, std::vector<Token> &pointerTokens, std::vector<Token> &pointerModifierTokens );
	bool ParseSpecificScopeInner(ASTPosition& cposition, std::vector<ASTTokenIndex> &insideBracketTokens, Token::Type tokenTypeL, Token::Type tokenTypeR, bool(*filterAllows)(Token& token) /*= &ASTPosition::FilterWhitespaceComments*/);
	bool ParseNTypeBase(ASTPosition &position, ASTType* typeNode);
	bool ParseNTypeIdentifier(ASTPosition &position, ASTType* typeNode);
	bool ParseNTypeFunctionPointer(ASTPosition &position, ASTType* typeNode);
	bool ParseNTypeSinglePointersAndReferences(ASTPosition &position, ASTType* typeNode, bool identifierScope=false);
	bool ParseNTypePointersAndReferences(ASTPosition &position, ASTType* typeNode, bool identifierScope=false);
	bool ParseNTypeArrayDefinitions(ASTPosition &position, ASTType* typeNode);

	bool ParseUnknown(ASTNode* parent, ASTPosition& position);
	bool ParseIgnored(ASTNode* parent, ASTPosition& position);
	bool ParseEndOfStream(ASTNode* parent, ASTPosition& position);
	
	bool ParseDeclarationSubArguments(ASTPosition &position, ASTNode* parent);
	bool ParseDeclarationSubArgumentsScoped(ASTPosition &position, ASTNode* parent, Token::Type leftScope, Token::Type rightScope);
	bool ParseDeclarationSubArgumentsScopedWithNonTypes(ASTPosition &cposition, ASTNode* parent, Token::Type leftScope, Token::Type rightScope);

	bool ParseExtensionAnnotation(ASTNode* parent, ASTPosition& cposition);
	bool ParseExtensionAnnotationContent(ASTTokenNode* ndAnnotationRoot, ASTPosition &position);
};
