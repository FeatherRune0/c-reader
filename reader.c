#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <clang-c/Index.h>

// to catch anonymous enums
char *lastEnumName = NULL;

// Used specifically for visiting enums
enum CXChildVisitResult visitEnum(CXCursor cursor, CXCursor parent, CXClientData clientData) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_EnumConstantDecl) {
        CXString constantNameStr = clang_getCursorSpelling(cursor);

        printf("\t\t%s = %lld\n",
            clang_getCString(constantNameStr),
            clang_getEnumConstantDeclValue(cursor)
        );

        clang_disposeString(constantNameStr);
    }

    return CXChildVisit_Recurse;
}

// Used specifically for visiting structures
enum CXChildVisitResult visitStructure(CXCursor cursor, CXCursor parent, CXClientData clientData) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_FieldDecl) {
        CXString fieldNameStr = clang_getCursorSpelling(cursor);
        CXType fieldType = clang_getCursorType(cursor);
        CXString fieldTypeStr = clang_getTypeSpelling(fieldType);

        printf("\t\t(%s) %s\n", clang_getCString(fieldTypeStr), clang_getCString(fieldNameStr));

        clang_disposeString(fieldTypeStr);
        clang_disposeString(fieldNameStr);
    }

    return CXChildVisit_Recurse;
}

// Used specifically for visiting function prototypes defined with a typedef
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

// general purpose visitor
enum CXChildVisitResult visit(CXCursor cursor, CXCursor parent, CXClientData clientData) {
    // avoid visiting nodes from any included libraries
    if (clang_Location_isFromMainFile(clang_getCursorLocation(cursor)) == 0) {
        return CXChildVisit_Continue;
    }

    // ref https://clang.llvm.org/doxygen/group__CINDEX__STRING.html
    
    // get the name of the current node
    CXString cursorStr = clang_getCursorSpelling(cursor);
    // get the kind of node (function declaration, struct declaration, etc)
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    CXString kindStr = clang_getCursorKindSpelling(kind);

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
    
    case CXCursor_StructDecl:
        printf("Struct %s\n", clang_getCString(cursorStr));
        // Parse the children of this node in order to get fields
        clang_visitChildren(
            cursor,
            &visitStructure,
            NULL
        );
    break;

    case CXCursor_EnumDecl:
    {
        CXString enumTypeStr = clang_getTypeSpelling(clang_getCursorType(cursor));
        
        char *enumName = NULL;
        if (strlen(clang_getCString(cursorStr)) == 0) {
            // anonymous enum does not have a name
            enumName = (char *) clang_getCString(enumTypeStr);
        } else {
            enumName = (char *) clang_getCString(cursorStr);
        }

        // Ensure we aren't re-visiting an anonymous enum
        if (lastEnumName == NULL || strcmp(lastEnumName, enumName) != 0) {
            if (lastEnumName != NULL) {
                free(lastEnumName);
            }
            lastEnumName = (char *) malloc(strlen(enumName));
            strcpy(lastEnumName, enumName);

            printf("Enum %s\n", enumName);
            // Parse the children of this node in order to get the constants
            clang_visitChildren(
                cursor,
                &visitEnum,
                NULL
            );
        }

        clang_disposeString(enumTypeStr);
        
        break;
    }

    case CXCursor_EnumConstantDecl:
    case CXCursor_FieldDecl:
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