#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cfg.hpp"
#include "re.hpp"

bool file_exists(std::string& filepath) {
    return access(filepath.c_str(), F_OK) == 0;
}
bool file_exists(const char* filepath) { return access(filepath, F_OK) == 0; }

size_t get_filesize(const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    struct stat stat;
    fstat(fileno(fp), &stat);
    fclose(fp);
    return stat.st_size;
}

std::string get_abspath(const char* relpath) {
    char actualpath[1000];
    realpath(relpath, actualpath);
    return std::string(actualpath);
}

// returns ptr to rest of string if newline is found, or nullptr if nothing left
char* getnewlinebound(char* str) {
    for (; *str != '\n' && *str != '\0'; ++str)
        ;
    if (*str == '\n') {
        return str;
    } else {
        // printf("found end of string\n");
        return nullptr;
    }
}

void printDiv(const char* str) {
    static int phaseno = 1;
    printf("\n-----------------------------------------------------------------"
           "---------------\n");
    printf(" Phase %d : %s\n", phaseno++, str);
    printf("-------------------------------------------------------------------"
           "-------------\n");
}

///////////////////////////////////////////////////////////////////////////////
#define DECL_TOKEN_TYPE(type) type,
enum TokenType {
#include "token_types.inc"
};
#undef DECL_TOKEN_TYPE

#define DECL_TOKEN_TYPE(type) #type,
const char* token_to_str[] = {
#include "token_types.inc"
};
#undef DECL_TOKEN_TYPE

struct Token {
    TokenType type = NUM_TOKEN_TYPES;
    const std::string str = "";
    int lineno = 0;
    int linepos = 0;
};

#define SINGLE_CHAR_TOKEN(__ch__, __token_type__)                              \
    case __ch__: {                                                             \
        tok(__token_type__, str(__ch__));                                      \
        break;                                                                 \
    }

#define KW_MATCH(__token_type__, __token_str__)                                \
    else if (re_match(idstr, "^" #__token_str__ "$")) {                        \
        return __token_type__;                                                 \
    }

struct Scanner {
    Scanner() = delete;
    Scanner(const char* buf) : _srcbuf(buf), _sz(strlen(buf)) {}
    std::vector<Token> scan() {
        char ch = *_srcbuf;
        _lineno = 0;
        _linepos = 0;
        while (ch != '\0') {
            switch (ch) {
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case '_': {
                // printf("%3ld: found alpha  char '%c'\n", ch);

                // TODO: implement keyword check
                // TokenType kw = parseKeyword();
                std::string idstr = consumeId();
                TokenType kw = getKeywordTokenType(idstr);
                tok(kw, idstr);
                break;
            }
            case '0' ... '9': {
                // printf("%3ld: found digit  char '%c'\n", ch);
                tok(NUM, consumeNum());
                break;
            }
            case '\n':
            case '\r':
            case '\t':
            case ' ': {
                if (ch == '\n') {
                    _lineno++;
                    _linepos = 0;
                }
                // printf("%3ld: found wspace char '%c'\n", i, ch);
                break;
            }
                SINGLE_CHAR_TOKEN('+', PLUS)
                SINGLE_CHAR_TOKEN('-', MINUS)
                SINGLE_CHAR_TOKEN('/', DIV)
                SINGLE_CHAR_TOKEN('*', MULT)
                SINGLE_CHAR_TOKEN('=', EQUALS)
                SINGLE_CHAR_TOKEN('!', BANG)
                SINGLE_CHAR_TOKEN(':', COLON)
                SINGLE_CHAR_TOKEN(';', SEMICOLON)
                SINGLE_CHAR_TOKEN('(', LEFT_PAREN)
                SINGLE_CHAR_TOKEN(')', RIGHT_PAREN)
                SINGLE_CHAR_TOKEN('{', LEFT_BRACE)
                SINGLE_CHAR_TOKEN('}', RIGHT_BRACE)
                SINGLE_CHAR_TOKEN('[', LEFT_BRACKET)
                SINGLE_CHAR_TOKEN(']', RIGHT_BRACKET)
            default: {
                printf("Found unimpl char '%c' (%d) at lineno %d, pos %d\n", ch,
                       ch, _lineno, _linepos);
                exit(0);
                break;
            }
            }
            ch = advance();
        }

        if (dump_token_stream)
            dumpTokenStream();

        std::vector<Token> tmp;
        std::swap(tmp, _tokens);
        return tmp;
    }
    TokenType getKeywordTokenType(const std::string& idstr) {
        if (false) {
        }
        KW_MATCH(AND, and)
        KW_MATCH(ELIF, elif)
        KW_MATCH(ELSE, else)
        KW_MATCH(CMP, cmp)
        KW_MATCH(FN, fn)
        KW_MATCH(FOR,for)
        KW_MATCH(IF, if)
        KW_MATCH(OR, or)
        KW_MATCH(RET, ret)
        KW_MATCH(TO, to)
        else {
            return ID;
        }
    }
    std::string consumeId() {
        std::string s;
        char ch = *_srcbuf;
        assert(isalpha(ch) or ch == '_');
        while (isalnum(ch) or ch == '_') {
            s.push_back(ch);
            ch = advance();
        }
        stepback();
        return s;
    }
    std::string consumeNum() {
        std::string s;
        char ch = *_srcbuf;
        assert(isdigit(ch));
        while (isdigit(ch)) {
            s.push_back(ch);
            ch = advance();
        }
        stepback();
        return s;
    }
    std::string str(char c) { return std::string(1, c); }
    void tok(TokenType type, std::string str) {
        _tokens.push_back({.type = type,
                           .str = std::move(str),
                           .lineno = _lineno,
                           .linepos = _linepos});
    }
    char advance() {
        _linepos++;
        return *(++_srcbuf);
    }
    char stepback() {
        _linepos--;
        return *(--_srcbuf);
    }

    void dumpTokenStream() {
        int curr_lineno = -1;

        for (auto tok : _tokens) {
            if (tok.lineno > curr_lineno) {
                curr_lineno = tok.lineno;
                printf("LINE %d: \n", curr_lineno);
            }
            printf("\t%-12s = %-10s at %d,%d  \n", token_to_str[tok.type],
                   tok.str.c_str(), tok.lineno, tok.linepos);
        }
    }

    std::vector<Token> _tokens;
    int _linepos = 0;
    int _lineno = 0;
    const char* _srcbuf = nullptr;
    const size_t _sz = 0;
};

#if 0

    parse functions fit into these categories:
        prefix
        infix

    infix and prefix  can exist for the same token type; this allows
    differentiating between, for example, '(' as a grouping operator 
    (prefix) and as a call operator (infix)  

    func    (       arg1, arg2)
     ^      ^       ^
     LHS    infix   RHS

    pratt parser core loop works like so:

    parse(prior_prec){
        tok = consume()
        prefix_func = get_prefix_function(tok)

        // fully parse expr starting at tok, recursively if needed
        expr = prefix_func(tok)

        // THE CORE TRICK
        // ---------------
        // if prior_prec >= next_prec that means that we should just return
        // immediately. The enclosing context binds stronger than the next
        // token and we should complete parsing the prior expr.
        //
        // if prior_prec < next_prec, the current expr *belongs* to the
        // expression that follows
        //
        next_prec = get_next_precedence()
        if (prior_prec < next_prec){
            return expr; 
        } 
        else {
            while (prior_precedence < getPredence(nexttok)){
                // next token must have a valid infix op defined, or prec would be 0!
                tok = consume()
                infix_func = get_infix_function(tok)

                // prev expr is subsumed within infix_func
                // this happens repeatedly until precedence stops it
                lhs = expr
                expr = infix_func(tok,lhs)
            }
            return expr
        }
    }

#endif

// fwd decl
struct Parser;

struct Expr {
    virtual void print() {}
    virtual ~Expr();
};
Expr::~Expr() {}

// literal expressions!
struct NameExpr : Expr {
    NameExpr(std::string name) : name(std::move(name)) {}
    std::string name;
    void print() { printf("Name[\"%s\"]", name.c_str()); }
};
struct NumExpr : Expr {
    NumExpr(double num) : num(num) {}
    double num;
    void print() { printf("Num[%g]", num); }
};
struct BinaryOpExpr : Expr {
    BinaryOpExpr() = delete;
    BinaryOpExpr(Expr* left, TokenType type, Expr* right)
        : left(left), type(type), right(right) {}
    Expr* left;
    TokenType type;
    Expr* right;
    void print() {
        printf("(");
        left->print();
        printf(" OP[%s] ", token_to_str[type]);
        right->print();
        printf(")");
    }
};

struct CallExpr : Expr {
    CallExpr() = delete;
    CallExpr(Expr* fn_name, std::vector<Expr*> args)
        : fn_name(fn_name), args(args) {}
    Expr* fn_name;
    std::vector<Expr*> args;
    void print() {
        printf("(");
        fn_name->print();
        printf(" CALL ");
        for (auto arg : args) {
            arg->print();
        }
        printf(")");
    }
};

// indexed by token type
typedef int Prec;
typedef std::vector<Token>::const_iterator TokIter;
typedef std::function<Expr*(Parser&)> PrefixFn;
typedef std::vector<std::pair<PrefixFn, Prec>> PrefixTable;
typedef std::function<Expr*(Parser&, Expr* left)> InfixFn;
typedef std::vector<std::pair<InfixFn, Prec>> InfixTable;

struct Parser {
    Parser() = delete;
    Parser(const std::vector<Token> tokens) : _tokens(std::move(tokens)) {
        _curr_token = _tokens.begin();

        initPrefixTable(_prefix_func_table);
        _prefix_func_table[ID] = std::make_pair(&Parser::parseID, 0);
        _prefix_func_table[NUM] = std::make_pair(&Parser::parseNum, 0);

        initInfixTable(_infix_func_table);
        _infix_func_table[PLUS] = std::make_pair(&Parser::parseBinaryOp, 1);
        _infix_func_table[MINUS] = std::make_pair(&Parser::parseBinaryOp, 2);
        _infix_func_table[MULT] = std::make_pair(&Parser::parseBinaryOp, 3);
        _infix_func_table[LEFT_PAREN] = std::make_pair(&Parser::parseCall, 10);
    }

    Expr* ParseExpr(int precedence) {
        auto c = _curr_token->type;
        Expr* expr = getPrefixFunc(c)(*this);

        while (precedence < getPrecedence()) {
            expr = getInfixFunc(_curr_token->type)(*this, expr);
        }

        return expr;
    }

    // avoid implementing precedence for now
    Prec getPrecedence() {
        if (_curr_token == _tokens.end()) {
            return -9999999;
        } else {
            return getInfixPrec(_curr_token->type);
        }
    }

    void initPrefixTable(PrefixTable& functable) {
        functable.resize(NUM_TOKEN_TYPES);
        std::fill(functable.begin(), functable.end(),
                  std::make_pair(&prefixboom, 0));
    }
    void initInfixTable(InfixTable& functable) {
        functable.resize(NUM_TOKEN_TYPES);
        std::fill(functable.begin(), functable.end(),
                  std::make_pair(&infixboom, 0));
    }

    PrefixFn getPrefixFunc(TokenType toktype) {
        assert(toktype < _prefix_func_table.size());
        return _prefix_func_table[toktype].first;
    }
    InfixFn getInfixFunc(TokenType toktype) {
        assert(toktype < _infix_func_table.size());
        return _infix_func_table[toktype].first;
    }
    Prec getPrefixPrec(TokenType toktype) {
        assert(toktype < _prefix_func_table.size());
        return _prefix_func_table[toktype].second;
    }
    Prec getInfixPrec(TokenType toktype) {
        assert(toktype < _infix_func_table.size());
        return _infix_func_table[toktype].second;
    }

    // return curr token and advance
    const Token& consume() { return *(_curr_token++); };
    PrefixTable _prefix_func_table;
    InfixTable _infix_func_table;
    std::vector<Token> _tokens;
    TokIter _curr_token;

    // prefix functions
    // NOTE: parsing functions must consume what they use!
    static NameExpr* parseID(Parser& parser) {
        return new NameExpr((parser.consume()).str);
    }
    static NumExpr* parseNum(Parser& parser) {
        return new NumExpr(atof(parser.consume().str.c_str()));
    }

    // infix functions
    // NOTE: parsing functions must consume what they use!
    static BinaryOpExpr* parseBinaryOp(Parser& parser, Expr* left) {
        TokenType type = parser.consume().type;
        // get right hand side
        auto right = parser.ParseExpr(parser.getInfixPrec(type));
        return new BinaryOpExpr(left, type, right);
    }
    static CallExpr* parseCall(Parser& parser, Expr* fn_name) {
        parser.consume(); // consume paren
        // XXX: should make this a CommaListExpr or something ... 
        auto args = parser.ParseExpr(parser.getInfixPrec(LEFT_PAREN));
        return new CallExpr(fn_name,{args});
    }

    static Expr* prefixboom(Parser&) {
        assert(0 && "this slot in function table is unimplemented");
        return new Expr();
    }
    static Expr* infixboom(Parser&, Expr*) {
        assert(0 && "this slot in function table is unimplemented");
        return new Expr();
    }
};

///////////////////////////////////////////////////////////////////////////////

void run_file(char* filepath, bool dump_source) {
    if (!file_exists(filepath)) {
        printf("Input file '%s' does not exist\n", filepath);
        return;
    }

    printf("compiling file '%s'\n", get_abspath(filepath).c_str());

    // read file into a buffer
    size_t filesize = get_filesize(filepath);
    char* filebuf = (char*)malloc(filesize + 1);
    FILE* fp = fopen(filepath, "r");
    fread(filebuf, sizeof(char), filesize, fp);

    // detect errors if any and close fp
    if (ferror(fp) != 0) {
        fprintf(stderr, "Error reading file into memory '%s'\n", filebuf);
        return;
    }
    filebuf[filesize] = '\0'; // make sure null terminated
    fclose(fp);

    // dump input to screen for debugging if requested
    if (dump_source) {
        printf("==========================================================="
               "===="
               "=================\n");
        char *startofline = filebuf, *endofline = filebuf;
        int lineno = 0;
        while ((endofline = getnewlinebound(startofline)) != nullptr) {
            printf("\u001b[36m%3d:\u001b[0m ", lineno);
            for (; startofline < endofline; ++startofline) {
                putchar(*startofline);
            }
            printf("\n");
            ++startofline; // move to char
            ++lineno;
        }
        printf("==========================================================="
               "===="
               "=================\n");
    }

    // get tokens from file
    printDiv("Scanner");
    Scanner scanner(filebuf);
    auto tokens = scanner.scan();

    printDiv("Parser");
    Parser parser(tokens);
    parser.ParseExpr(0)->print();

    printDiv("cleanup");
}

void run_prompt() { printf("prompt goes here\n"); }

int main(int argc, char** argv) {

    if (argc == 2) {
        run_file(argv[1], true);
    } else if (argc == 1) {
        run_prompt();
    } else {
        printf("too many args!\n");
    }

    return 0;
}
