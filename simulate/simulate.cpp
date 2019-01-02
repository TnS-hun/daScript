#include "precomp.h"

#include "simulate.h"
#include "runtime_string.h"

namespace yzg
{
    // SimNode_MakeBlock
    
    __m128 SimNode_MakeBlock::eval ( Context & context )  {
        Block block;
        block.stackOffset = uint32_t(context.stackTop - context.stack);
        block.argumentsOffset = argStackTop ? uint32_t(context.stackTop + argStackTop - context.stack) : 0;
        block.body = subexpr;
        return cast<Block>::from(block);
    }
    
    // SimNode_At
    
    __m128 SimNode_At::eval ( Context & context )  {
        auto pValue = value->evalPtr(context);
        YZG_EXCEPTION_POINT;
        uint32_t idx = cast<uint32_t>::to(index->eval(context));
        YZG_EXCEPTION_POINT;
        if ( idx >= range ) {
            context.throw_error("index out of range");
            return _mm_setzero_ps();
        } else {
            return cast<char *>::from(pValue + idx*stride);
        }
    }
    
    // SimNode_Call
    
    void SimNode_Call::evalArgs ( Context & context, __m128 * argValues ) {
        for ( int i=0; i!=nArguments && !context.stopFlags; ++i ) {
            argValues[i] = arguments[i]->eval(context);
        }
    }
    
    __m128 SimNode_Call::eval ( Context & context ) {
		__m128 * argValues = (__m128 *)(alloca(nArguments * sizeof(__m128)));
        evalArgs(context, argValues);
        YZG_EXCEPTION_POINT;
        return context.call(fnIndex, argValues, debug.line);
    }
    
    // SimNode_Invoke
    
    __m128 SimNode_Invoke::eval ( Context & context )  {
        __m128 * argValues = (__m128 *)(alloca(nArguments * sizeof(__m128)));
        evalArgs(context, argValues);
        YZG_EXCEPTION_POINT;
        Block block = cast<Block>::to(argValues[0]);
        if ( nArguments>1 ) {
            return context.invoke(block, argValues + 1);
        } else {
            return context.invoke(block, nullptr);
        }
    }
    
    // SimNode_Debug
    
    __m128 SimNode_Debug::eval ( Context & context ) {
        __m128 res = subexpr->eval(context);
        YZG_EXCEPTION_POINT;
        stringstream ssw;
        if ( message ) ssw << message << " ";
        ssw << debug_type(typeInfo) << " = " << debug_value(res, typeInfo) << " at " << debug.describe() << "\n";
        context.to_out(ssw.str().c_str());
        return res;
    }
    
    // SimNode_Assert
    
    __m128 SimNode_Assert::eval ( Context & context ) {
        if ( !cast<bool>::to(subexpr->eval(context)) ) {
            YZG_EXCEPTION_POINT;
            string error_message = "assert failed";
            if ( message )
                error_message = error_message + ", " + message;
            string error = reportError(context.debugInput, debug.line, debug.column, error_message );
            context.to_err(error.c_str());
            context.throw_error("assert failed");
        }
        return _mm_setzero_ps();
    }
    
    // SimNode_TryCatch
    
    __m128 SimNode_TryCatch::eval ( Context & context ) {
        #if YZG_ENABLE_EXCEPTIONS
            try_block->eval(context);
            if ( context.stopFlags & EvalFlags::stopForThrow ) {
                context.stopFlags &= ~(EvalFlags::stopForThrow | EvalFlags::stopForReturn | EvalFlags::stopForBreak);
                catch_block->eval(context);
            }
        #else
            try {
                try_block->eval(context);
            } catch ( const runtime_error & ) {
                context.stopFlags &= ~(EvalFlags::stopForThrow | EvalFlags::stopForReturn | EvalFlags::stopForBreak);
                catch_block->eval(context);
            }
        #endif
        return _mm_setzero_ps();
    }
    
    // SimNode_Ptr2Ref
    
    __m128 SimNode_Ptr2Ref::eval ( Context & context ) {
        auto ptr = subexpr->evalPtr(context);
        YZG_EXCEPTION_POINT;
        if ( ptr == nullptr ) {
            context.throw_error("dereferencing null pointer");
            return _mm_setzero_ps();
        }
        return cast<char *>::from(ptr);
    }
    
    // SimNode_Ptr2Ref
    
    __m128 SimNode_NullCoalescingRef::eval ( Context & context ) {
        auto ptr = subexpr->evalPtr(context);
        YZG_EXCEPTION_POINT;
        return ptr ? cast<char *>::from(ptr) : value->eval(context);
    }
    
    // FIELD
    
    __m128 SimNode_FieldDeref::eval ( Context & context ) {
        auto prv = value->evalPtr(context);
        YZG_EXCEPTION_POINT;
        if ( prv ) {
            return cast<char *>::from( prv + offset );
        } else {
            context.throw_error("dereferencing null pointer");
            return _mm_setzero_ps();
        }
    }
    
    // FIELD ?.
    
    __m128 SimNode_SafeFieldDeref::eval ( Context & context ) {
        auto prv = value->evalPtr(context);
        YZG_EXCEPTION_POINT;
        if ( prv ) {
            return cast<char *>::from( prv + offset );
        } else {
            return _mm_setzero_ps();
        }
    }
    
    // FIELD ?. ->
    
    __m128 SimNode_SafeFieldDerefPtr::eval ( Context & context ) {
        char ** prv = (char **) value->evalPtr(context);
        YZG_EXCEPTION_POINT;
        if ( prv ) {
            return cast<char *>::from( *(prv + offset) );
        } else {
            return _mm_setzero_ps();
        }
    }
    
    // SimNode_New
    
    __m128 SimNode_New::eval ( Context & context ) {
        if ( void * ptr = context.allocate(bytes) ) {
            memset ( ptr, 0, bytes );
            return cast<void *>::from(ptr);
        } else {
            context.throw_error("out of memory");
            return _mm_setzero_ps();
        }
    }
    
    // SimNode_CopyRefValue
    
    __m128 SimNode_CopyRefValue::eval ( Context & context ) {
        auto pl = l->evalPtr(context);
        YZG_EXCEPTION_POINT;
        auto pr = r->evalPtr(context);
        YZG_EXCEPTION_POINT;
        memcpy ( pl, pr, size );
        return cast<char *>::from(pl);
    }
    
    // SimNode_MoveRefValue
    
    __m128 SimNode_MoveRefValue::eval ( Context & context ) {
        auto pl = l->evalPtr(context);
        YZG_EXCEPTION_POINT;
        auto pr = r->evalPtr(context);
        YZG_EXCEPTION_POINT;
        memcpy ( pl, pr, size );
        memset ( pr, 0, size );
        return cast<char *>::from(pl);
    }
    
    // SimNode_Block
    
    __m128 SimNode_Block::eval ( Context & context ) {
        for ( int i = 0; i!=total && !context.stopFlags; ++i )
            list[i]->eval(context);
        return _mm_setzero_ps();
    }
    
    __m128 SimNode_ClosureBlock::eval ( Context & context ) {
        for ( int i = 0; i!=total && !context.stopFlags; ++i )
            list[i]->eval(context);
        if ( context.stopFlags & EvalFlags::stopForReturn ) {
            context.stopFlags &= ~EvalFlags::stopForReturn;
            return context.abiResult();
        } else {
            if ( needResult ) context.throw_error("end of block without return");
            return _mm_setzero_ps();
        }
    }
    
    // SimNode_Let
    
    __m128 SimNode_Let::eval ( Context & context ) {
        for ( int i = 0; i!=total && !context.stopFlags; ++i )
            list[i]->eval(context);
        YZG_EXCEPTION_POINT;
        return subexpr ? subexpr->eval(context) : _mm_setzero_ps();
    }
    
    // SimNode_IfThenElse
    
    __m128 SimNode_IfThenElse::eval ( Context & context ) {
        bool cmp = cast<bool>::to(cond->eval(context));
        YZG_EXCEPTION_POINT;
        if ( cmp ) {
            return if_true->eval(context);
        } else if ( if_false ) {
            return if_false->eval(context);
        } else {
            return _mm_setzero_ps();
        }
    }
    
    // SimNode_While
    
    __m128 SimNode_While::eval ( Context & context ) {
        while ( cast<bool>::to(cond->eval(context)) && !context.stopFlags ) {
            body->eval(context);
        }
        context.stopFlags &= ~EvalFlags::stopForBreak;
        return _mm_setzero_ps();
    }
    
    // Context
    
    Context::Context(const string * lines, int las) {
        linearAllocatorSize = las;
        linearAllocator = linearAllocatorBase = (char *) _mm_malloc(linearAllocatorSize, 16);
        stack = (char *) _mm_malloc(stackSize, 16);
        stackTop = stack + stackSize;
        debugInput = lines;
    }
    
    Context::~Context() {
        _mm_free(linearAllocatorBase);
        _mm_free(stack);
    }
    
    void * Context::reallocate ( void * oldData, uint32_t oldSize, uint32_t size ) {
        if ( size <= oldSize ) return oldData;
        size = (size + 0x0f) & ~0x0f;
        oldSize = (oldSize + 0x0f) & ~0x0f;
        if ( oldData && (oldData == linearAllocator - oldSize) ) {
            uint32_t extra = size - oldSize;
            if ( linearAllocator - linearAllocatorBase + extra > linearAllocatorSize ) {
                throw_error("out of linear allocator space");
                return nullptr;
            }
            linearAllocator += extra;
            return oldData;
        } else {
            return allocate(size);
        }
    }
    
    void * Context::allocate ( uint32_t size ) {
        size = (size + 0x0f) & ~0x0f;
        if ( linearAllocator - linearAllocatorBase + size > linearAllocatorSize ) {
            throw_error("out of linear allocator space");
            return nullptr;
        }
        void * result = linearAllocator;
        linearAllocator += size;
        return result;
    }
    
    char * Context::allocateName ( const string & name ) {
        if ( name.empty() )
            return nullptr;
        auto size = name.length() + 1;
        if ( char * str = (char *) allocate(uint32_t(size+1)) ) {
            memcpy ( str, name.c_str(), size );
            return str;
        } else {
            return nullptr;
        }
    }
    __m128 Context::invoke(const Block &block, __m128 * args ) {
        char * saveSp = stackTop;
        char * saveISp = invokeStackTop;
        invokeStackTop = stackTop;
        stackTop = stack + block.stackOffset;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        __m128 ** pArgs;
        __m128 * saveArgs;
        if ( block.argumentsOffset ) {
            assert(args && "expecting arguments");
            pArgs = (__m128 **)(stack + block.argumentsOffset);
            saveArgs = *pArgs;
            *pArgs = args;
        } else {
            assert(!args && "not expecting arguments");
        }
        // cout << "invoke , stack at " << (context.stack + context.stackSize - context.stackTop) << endl;
        __m128 result = block.body->eval(*this);
        if ( args && block.argumentsOffset ) {
            *pArgs = saveArgs;
        }
        invokeStackTop = saveISp;
        stackTop = saveSp;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        return result;
    }
    
    __m128 Context::invokeEx(const Block &block, __m128 * args, function<void (SimNode *)> && when) {
        char * saveSp = stackTop;
        char * saveISp = invokeStackTop;
        invokeStackTop = stackTop;
        stackTop = stack + block.stackOffset;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        __m128 ** pArgs;
        __m128 * saveArgs;
        if ( block.argumentsOffset ) {
            assert(args && "expecting arguments");
            pArgs = (__m128 **)(stack + block.argumentsOffset);
            saveArgs = *pArgs;
            *pArgs = args;
        } else {
            assert(!args && "not expecting arguments");
        }
        // cout << "invoke , stack at " << (context.stack + context.stackSize - context.stackTop) << endl;
        when(block.body);
        __m128 result = abiResult();
        if ( args && block.argumentsOffset ) {
            *pArgs = saveArgs;
        }
        invokeStackTop = saveISp;
        stackTop = saveSp;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        return result;
    }

    __m128 Context::call ( int fnIndex, __m128 * args, int line ) {
        assert(fnIndex>=0 && fnIndex<totalFunctions && "function index out of range");
        auto & fn = functions[fnIndex];
        // PUSH
        char * top = invokeStackTop ? invokeStackTop : stackTop;
        if ( stack - ( top - fn.stackSize ) > stackSize ) {
            throw_error("stack overflow");
            return _mm_setzero_ps();
        }
        char * pushStack = stackTop;
        char * pushInvokeStack = invokeStackTop;
        stackTop = top - fn.stackSize;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        invokeStackTop = nullptr;
        // cout << "call " << fn.debug->name <<  ", stack at " << (stack + stackSize - stackTop) << endl;
        // fill prologue
        Prologue * pp = (Prologue *) stackTop;
        pp->result =        _mm_setzero_ps();
        pp->arguments =     args;
#if YZG_ENABLE_STACK_WALK
        pp->info =          fn.debug;
        pp->line =          line;
#endif
        // CALL
        fn.code->eval(*this);
        __m128 result = abiResult();
        stopFlags &= ~(EvalFlags::stopForReturn | EvalFlags::stopForBreak);
        // POP
        invokeStackTop = pushInvokeStack;
        stackTop = pushStack;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        return result;
    }

    __m128 Context::callEx(int fnIndex, __m128 *args, int line, function<void (SimNode *)> && when) {
        assert(fnIndex>=0 && fnIndex<totalFunctions && "function index out of range");
        auto & fn = functions[fnIndex];
        // PUSH
        char * top = invokeStackTop ? invokeStackTop : stackTop;
        if ( stack - ( top - fn.stackSize ) > stackSize ) {
            throw_error("stack overflow");
            return _mm_setzero_ps();
        }
        char * pushStack = stackTop;
        char * pushInvokeStack = invokeStackTop;
        stackTop = top - fn.stackSize;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        invokeStackTop = nullptr;
        // cout << "call " << fn.debug->name <<  ", stack at " << (stack + stackSize - stackTop) << endl;
        // fill prologue
        Prologue * pp = (Prologue *) stackTop;
        pp->result =        _mm_setzero_ps();
        pp->arguments =     args;
#if YZG_ENABLE_STACK_WALK
        pp->info =          fn.debug;
        pp->line =          line;
#endif
        // CALL
        when(fn.code);
        __m128 result = abiResult();
        stopFlags &= ~(EvalFlags::stopForReturn | EvalFlags::stopForBreak);
        // POP
        invokeStackTop = pushInvokeStack;
        stackTop = pushStack;
        assert ( stackTop >= stack && stackTop < stackTop + stackSize );
        return result;
    }

    
    void Context::runInitScript ( void ) {
        for ( int i=0; i!=totalVariables && !stopFlags; ++i ) {
            auto & pv = globalVariables[i];
            if ( pv.init ) {
                pv.init->eval(*this);
            } else {
                memset ( pv.value.dataPtr, 0, pv.size );
            }
        }
    }
    
    int Context::findFunction ( const char * name ) const {
        for ( int fni = 0; fni != totalFunctions; ++fni ) {
            if ( strcmp(functions[fni].name, name)==0 ) {
                return fni;
            }
        }
        return -1;
    }
    
    int Context::findVariable ( const char * name ) const {
        for ( int vni = 0; vni != totalVariables; ++vni ) {
            if ( strcmp(globalVariables[vni].name, name)==0 ) {
                return vni;
            }
        }
        return -1;
    }
    
    void Context::stackWalk() {
        stringstream ssw;
    #if YZG_ENABLE_STACK_WALK
        ssw << "\nCALL STACK (sp=" << (stack + stackSize - stackTop) << "):\n";
        char * sp = stackTop;
        while ( sp>=stackTop && sp <(stack+stackSize) ) {
            int isp = int(stack + stackSize - sp);
            Prologue * pp = (Prologue *) sp;
            if ( pp->line ) {
                ssw << pp->info->name << " at line " << pp->line << " (sp=" << isp << ")\n";
            } else {
                ssw << pp->info->name << "(sp=" << isp << ")\n";
            }
            for ( uint32_t i = 0; i != pp->info->argsSize; ++i ) {
                ssw << "\t" << pp->info->args[i]->name
                    << " : " << debug_type(pp->info->args[i])
                    << " = \t" << debug_value(pp->arguments[i], pp->info->args[i]) << "\n";
            }
            sp += pp->info->stackSize;
        }
        ssw << "\n";
    #else
        ssw << "\nCALL STACK TRACKING DISABLED:\n\n";
    #endif
       to_out(ssw.str().c_str());
    }
    
    void Context::breakPoint(int column, int line) const {
#ifdef _MSC_VER
		__debugbreak();
#else
        raise(SIGTRAP);
#endif
    }
    
    void Context::to_out ( const char * message ) {
        cout << message;
    }
    
    void Context::to_err ( const char * message ) {
        cerr << message;
    }
}
