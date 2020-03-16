#include <stdio.h>
#include <string.h>
#include <clang-c/Index.h>

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
        printf("Return Type: %s\n", clang_getCString(returnTypeStr));
        clang_disposeString(returnTypeStr);

        int nArgs = clang_Cursor_getNumArguments(cursor);
        
        printf("Arguments: %d\n", nArgs);
        for (int i = 0; i < nArgs; i++) {
            CXCursor argument = clang_Cursor_getArgument(cursor, i);
            CXType argumentType = clang_getCursorType(argument);
            CXString argumentTypeStr = clang_getTypeSpelling(argumentType);
            CXString argumentStr = clang_getCursorSpelling(argument);
            
            printf("(%s) %s\n", clang_getCString(argumentTypeStr), clang_getCString(argumentStr));

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

    case CXCursor_EnumDecl:
        // do nothing as this can be handled from the constant declarations
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