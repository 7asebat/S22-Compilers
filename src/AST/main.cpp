#include "AST.h"

int
main(int argc, char* argv[])
{
    using namespace AST;

	// -2 + 4
    Operation_Binary binary = { Operation_Binary::ADD };
	{
        binary.left = Node::create<Operation_Unary>(Operation_Unary::NEG);
		{
			auto &unary = binary.left->as_unary;
            unary.right = Node::create<Constant>(2);
        }
    }
	{
        binary.right = Node::create<Constant>(4);
    }

    return 0;
}