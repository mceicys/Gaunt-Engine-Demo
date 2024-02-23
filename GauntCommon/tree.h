// tree.h
// Martynas Ceicys

namespace com
{

/*--------------------------------------
	com::TraverseTree

Returns the next node of n in pre-order (node, left, right) traversal. node must have left,
right, and parent pointers.
--------------------------------------*/
template <class Node> const Node* TraverseTree(const Node* n)
{
	if(n->left)
		return (const Node*)n->left;
	else if(n->right)
		return (const Node*)n->right;

	while(n->parent)
	{
		if(n->parent->left == n)
			return (const Node*)n->parent->right;
		else
			n = (const Node*)n->parent;
	}

	return 0;
}

template <class Node> Node* TraverseTree(Node* n)
{
	return const_cast<Node*>(TraverseTree(const_cast<const Node*>(n)));
}

/*--------------------------------------
	com::TraverseTreePost

left, right, node. Do LeftLeaf(root) to start a full traversal.
--------------------------------------*/
template <class Node> const Node* TraverseTreePost(const Node* n)
{
	if(!n->parent)
		return 0;

	if(n->parent->left == n && n->parent->right)
	{
		for(n = n->parent->right; n->left; n = n->left);
	}
	else
		return n->parent;

	return n;
}

template <class Node> Node* TraverseTreePost(Node* n)
{
	return const_cast<Node*>(TraverseTreePost(const_cast<const Node*>(n)));
}

/*--------------------------------------
	com::LeftLeaf
--------------------------------------*/
template <class Node> const Node* LeftLeaf(const Node* n)
{
	if(!n)
		return n;

	while(n->left)
		n = n->left;
	return n;
}

template <class Node> Node* LeftLeaf(Node* n)
{
	return const_cast<Node*>(LeftLeaf(const_cast<const Node*>(n)));
}

/*--------------------------------------
	com::NumTreeLevels
--------------------------------------*/
template <class Node> unsigned NumTreeLevels(const Node* root)
{
	unsigned l = 0;

	const Node* n = root;
	do
	{
		if(!n->left && !n->right)
		{
			unsigned i = 0;
			const Node* m = n;

			while(m)
			{
				m = (const Node*)m->parent;
				i++;
			}

			if(i > l)
				l = i;
		}
	} while(n = com::TraverseTree(n));

	return l;
}

/*--------------------------------------
	com::NodeLevel
--------------------------------------*/
template <class Node> unsigned NodeLevel(const Node* n)
{
	unsigned level = 0;
	for(; n->parent; level++, n = n->parent);
	return level;
}

/*--------------------------------------
	com::TraceNode

0 = left, 1 = right
pathOut must have NodeLevel(n) elements.
--------------------------------------*/
template <class Node> void TraceNode(const Node* n, unsigned* pathOut)
{
	unsigned level = NodeLevel(n);
	for(; n->parent; n = n->parent)
	{
		pathOut[--level] = n->parent->right == n ? 1 : 0;
	}
}

/*--------------------------------------
	com::NumNodes
--------------------------------------*/
template <class Node> unsigned NumNodes(const Node* root)
{
	unsigned numNodes = 0;
	for(const Node* n = root; n; n = TraverseTree(n))
		numNodes++;
	return numNodes;
}

/*--------------------------------------
	com::NumLeaves
--------------------------------------*/
template <class Node> unsigned NumLeaves(const Node* root)
{
	unsigned numLeaves = 0;
	for(const Node* n = root; n; n = TraverseTree(n))
		numLeaves += !n->left;
	return numLeaves;
}

}