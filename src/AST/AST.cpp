#include "AST.h"

namespace AST
{
	Operation_Assignment::~Operation_Assignment()
	{
        free_destruct(this->left);
        free_destruct(this->right);
	}

	Operation_Binary::~Operation_Binary()
	{
        free_destruct(this->left);
        free_destruct(this->right);
	}

	Operation_Unary::~Operation_Unary()
	{
        free_destruct(this->right);
    }

	Node::~Node()
	{
		switch (this->kind)
		{
		case CONSTANT:
		case IDENTIFIER:
            break;

        case OP_BINARY:
            this->as_binary.~Operation_Binary();
            break;
        case OP_ASSIGN:
            this->as_assign.~Operation_Assignment();
            break;
		case OP_UNARY:
            this->as_unary.~Operation_Unary();
            break;
        }
    }
}
