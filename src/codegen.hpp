#pragma once

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctype.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cfg.hpp"
#include "color.hpp"
#include "err.hpp"
#include "re.hpp"
#include "scan.hpp"
#include "expr.hpp"
#include "vm.hpp"

struct CodeGen {
    CodeGen(std::vector<Expr*>& stmts): stmts(stmts) { }

    Chunk genCode(){
        Chunk code;
        for (const auto stmtexpr : stmts){
            Chunk exprcode;
            // expr should always leave stack idx at +1
            stmtexpr->codegen(exprcode);
            // ... so pop at end of stmt to restore stack
            exprcode.addOp(OP_POP);
            VM vm(exprcode);
            vm.run();
        }
        return code; 
    }

    std::vector<Expr*>& stmts;
};
