#include <stdio.h>
#include <string.h>
#include <clang-c/Index.h>

enum CXChildVisitResult visitFunctionProto(CXCursor cursor, CXCursor parent, CXClientData clientData) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_ParmDecl) {
        CXString parmNameStr = clang_getCursorSpelling(cursor);
        CXType parmType = clang_getCursorType(cursor);
        CXString parmTypeStr = clang_getTypeSpelling(parmType);

        printf("\t\t(%s) %s\n", clang_getCString(parmTypeStr), clang_getCString(parmNameStr));

        clang_disposeString(parmTypeStr);
        clang_disposeString(parmNameStr);
    }

    return CXChildVisit_Recurse;
}

enum CXChildVisitResult visit(CXCursor cursor, CXCursor parent, CXClientData clientData) {
    // ref https://clang.llvm.org/doxygen/group__CINDEX__STRING.html
    CXString cursorStr = clang_getCursorSpelling(cursor);
    CXString kindStr = clang_getCursorKindSpelling(clang_getCursorKind(cursor));
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    switch (kind)
    {
    case CXCursor_FunctionDecl:
        printf("Function %s\n", clang_getCString(cursorStr));
        
        CXType returnType = clang_getCursorResultType(cursor);
        CXString returnTypeStr = clang_getTypeSpelling(returnType);
        printf("\tReturns: %s\n", clang_getCString(returnTypeStr));
        clang_disposeString(returnTypeStr);

        int nArgs = clang_Cursor_getNumArguments(cursor);
        
        printf("\tArguments: %d\n", nArgs);
        for (int i = 0; i < nArgs; i++) {
            CXCursor argument = clang_Cursor_getArgument(cursor, i);
            CXType argumentType = clang_getCursorType(argument);
            CXString argumentTypeStr = clang_getTypeSpelling(argumentType);
            CXString argumentStr = clang_getCursorSpelling(argument);
            
            printf("\t\t(%s) %s\n", clang_getCString(argumentTypeStr), clang_getCString(argumentStr));

            clang_disposeString(argumentTypeStr);
            clang_disposeString(argumentStr);
        }

        break;

    case CXCursor_EnumConstantDecl:
    {
        CXCursor enumConstDecl = clang_getCursorDefinition(parent);
        CXString enumConstDeclStr = clang_getCursorSpelling(enumConstDecl);
        const char *enumName = clang_getCString(enumConstDeclStr);
        if (strlen(enumName) > 0) {
            printf("From %s", enumName);
        } else {
            printf("From anonymous enum");
        }
        printf(", constant %s = %lld\n",
            clang_getCString(cursorStr),
            clang_getEnumConstantDeclValue(cursor)
        );

        clang_disposeString(enumConstDeclStr);
        break;
    }

    case CXCursor_TypedefDecl:
        printf("Typedef");
        CXType underlyingType = clang_getTypedefDeclUnderlyingType(cursor);
        CXType newType = clang_getCursorType(cursor);
        CXString underlyingTypeStr = clang_getTypeSpelling(underlyingType);
        CXString newTypeStr = clang_getTypeSpelling(newType);
        printf(": %s => %s\n", clang_getCString(newTypeStr), clang_getCString(underlyingTypeStr));
        
        CXType resultType = clang_getResultType(clang_getPointeeType(underlyingType));
        CXString resultTypeStr = clang_getTypeSpelling(resultType);

        CXType canonicalType = clang_getCanonicalType(clang_getPointeeType(underlyingType));
        
        nArgs = clang_getNumArgTypes(canonicalType);
        if (nArgs != -1) {
            // function types only
            printf("\tReturns : %s\n\tArguments : %d\n",
                clang_getCString(resultTypeStr),
                nArgs
            );
            // Parse the children of this node in order to get parameter names
            clang_visitChildren(
                cursor,
                &visitFunctionProto,
                NULL
            );
        }

        clang_disposeString(resultTypeStr);

        clang_disposeString(newTypeStr);
        clang_disposeString(underlyingTypeStr);
        break;

    case CXCursor_EnumDecl:
        // do nothing as this can be handled from the constant declarations
    case CXCursor_TypeRef:
        // as we are already getting the types and we can get the cursor
        // to the declaration if necessary
    case CXCursor_ParmDecl:
    case CXCursor_UnaryOperator:
    case CXCursor_IntegerLiteral:
        break;

    default:
        printf("> %s of %s \n", clang_getCString(cursorStr), clang_getCString(kindStr));
        break;
    }

    clang_disposeString(kindStr);
    clang_disposeString(cursorStr);

    return CXChildVisit_Recurse;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <source file>\n", argv[0]);
        return -1;
    }
    // Ref https://clang.llvm.org/doxygen/group__CINDEX__TRANSLATION__UNIT.html
    // Create index for creating the translation units.
    CXIndex index = clang_createIndex(0, 0);

    // Create the translation unit from the source file
    CXTranslationUnit translationUnit = clang_parseTranslationUnit(
        index,
        argv[1],
        NULL, 0, NULL, 0,
        CXTranslationUnit_None
    );

    if (translationUnit == NULL) {
        printf("Failed to create translation unit\n");
        return -1;
    }

    // Ref https://clang.llvm.org/doxygen/group__CINDEX__CURSOR__MANIP.html
    CXCursor cursor = clang_getTranslationUnitCursor(translationUnit);

    // Ref https://clang.llvm.org/doxygen/group__CINDEX__CURSOR__TRAVERSAL.html
    clang_visitChildren(
        cursor,
        &visit,
        NULL
    );

    clang_disposeTranslationUnit(translationUnit);
    clang_disposeIndex(index);

    return 0;
}