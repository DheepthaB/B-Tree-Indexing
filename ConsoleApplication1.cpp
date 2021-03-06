// ConsoleApplication1.cpp : Defines the entry point for the console application.

/*
This program builds a b+ tree index file named 'index.idx' for a data file named 'TestData.txt'
The program reads the data from TestData.txt as a block of data and writes it out into 
index.idx as a block of data. Each block has a size of 1024 bytes.

Written by Dheeptha Badrinarayanan at The University of Texas at Dallas for the purpose
of a class assignemnt.
*/

#include "stdafx.h"
#include<vector>
#include<stdio.h>
#include<iostream>
#include<fstream>
#include<string>
#include <algorithm> 

#define HEADER_BLOCK_OFFSET 0
#define FIRST_RECORD_OFFSET 1025
#define DELETE_PTR(x) { if(x!=NULL) delete x;}

using namespace std;

//meta data for the b+ tree index
struct Header {
	char dataFile[256];
	int keySize;
	long  rootPointer;
	long leafPointer;
};

//structure of interior nodes in the b+ tree
struct InteriorNode {
	vector<long> indexPointers;
	vector<string> keys;
};

//structure of leaf nodes in the b+ tree
struct LeafNode {
	vector<long> textPointers;
	vector<string> keys;
	long nextLeafPointer;
};

//structure of the b+ tree
class BPlusTree {
public:
	Header *header;
	vector<InteriorNode*> interiorNodes;
	vector<LeafNode*> leafNodes;
	string indexFile;
	vector<long> treeStack;
	vector<long> leafNodeOffsets;
	LeafNode* currentLeaf;

	~BPlusTree();
	LeafNode *getEmptyLeafNode();
	void insertIntoLeafNode(LeafNode *lf, string key, long ptr);
	void splitNode(LeafNode *lf, string key, long ptr);
	long writingLeafToFile(LeafNode *ln, long ptrtoleaf);
	void addLeafOffset(long ptr);
	LeafNode* returnLeafNode(string indexfile, long ptr);
	InteriorNode* returnInteriorNode(string indexfile, long ptr);
	void insertInteriorNode(InteriorNode *in, string key, long ptr);
	InteriorNode* getEmptyInteriorNode();
	long writingInteriorToFile(string indexfile, InteriorNode* in, long offset);
	LeafNode* search(InteriorNode* node, string key);
	bool isLeafNode(long ptr);
	long getRightTreePointer(InteriorNode node, string key);
	long getLeftTreePointer(InteriorNode node, string key);
};

BPlusTree::~BPlusTree() {
	DELETE_PTR(header);
	DELETE_PTR(currentLeaf);
	for (int i = 0; i < interiorNodes.size(); i++)
	{
		DELETE_PTR(interiorNodes.at(i));
	}
}

LeafNode* BPlusTree::getEmptyLeafNode() {
	int maxSize = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;  //44 key+textPointer per block
	LeafNode *lf = new LeafNode();
	lf->keys.resize(maxSize);
	lf->textPointers.resize(maxSize);
	lf->nextLeafPointer = 55555;
	return lf;
}

long BPlusTree::writingLeafToFile(LeafNode *ln, long ptrtoleaf) {
	fstream outputfile;
	outputfile.open(indexFile, ios::out | ios::binary);
	int size = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;
	ln->keys.resize(size);
	ln->textPointers.resize(size);

	long writepointer;
	if (ptrtoleaf == 0) {
		outputfile.seekp(ptrtoleaf);
		writepointer = ptrtoleaf;
	}
	else {
		outputfile.seekp(outputfile.end);
		writepointer =(long) outputfile.tellp();
	}

	outputfile.write(reinterpret_cast<char*>(&ln->keys[0]), (ln->keys.size() * sizeof(ln->keys[0])));
	outputfile.write((char*)(&(ln->textPointers[0])), (ln->textPointers.size() * sizeof(ln->textPointers[0])));
	outputfile.write((char*)&ln->nextLeafPointer, sizeof(long));
	outputfile.close();

	return writepointer;
}

void BPlusTree::addLeafOffset(long ptr) {
	if (find(leafNodeOffsets.begin(), leafNodeOffsets.end(), ptr) == leafNodeOffsets.end())
	{
		leafNodeOffsets.push_back(ptr);
	}
}

long BPlusTree::writingInteriorToFile(string indexfile, InteriorNode* in, long offset) {
	fstream outputfile;
	outputfile.open(indexFile, ios::out | ios::binary);

	int size = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;
	in->keys.resize(size);
	in->indexPointers.resize(size + 1);


	long writeoffset;
	if (offset != 0) {
		outputfile.seekp(offset);
		writeoffset = offset;
	}
	else {
		outputfile.seekp(outputfile.end);
		writeoffset =(long) outputfile.tellp();
	}

	outputfile.write(reinterpret_cast<char*>(&in->keys[0]), (in->keys.size() * sizeof(in->keys[0])));

	outputfile.write((char*)(&in->indexPointers[0]), (in->indexPointers.size() * sizeof(in->indexPointers[0])));

	outputfile.close();
	return writeoffset;
}

InteriorNode* BPlusTree::getEmptyInteriorNode() {
	int maxSize = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;
	InteriorNode * in = new InteriorNode();
	in->keys.resize(maxSize);
	in->indexPointers.resize(maxSize + 1);
	return in;
}

InteriorNode* BPlusTree::returnInteriorNode(string indexfile, long ptr) {
	fstream inputfile;
	inputfile.open(indexFile, ios::in || ios::binary);
	InteriorNode* in = getEmptyInteriorNode();

	inputfile.seekg(ptr);
	//read index pointers and keys
	size_t bytesTobeRead = in->keys.size() * (sizeof(in->keys[0]));
	inputfile.read((char*)(&in->keys[0]), bytesTobeRead);
	inputfile.read((char*)(&in->indexPointers[0]), (in->indexPointers.size() * sizeof(long)));

	inputfile.close();

	return in;
}

LeafNode* BPlusTree::returnLeafNode(string indexfile, long offset)
{
	fstream inputfile;
	int maxSize = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;

	LeafNode *lf = getEmptyLeafNode();
	lf->keys.resize(maxSize);
	lf->textPointers.resize(maxSize);

	inputfile.open(indexFile, ios::in | ios::binary);

	inputfile.seekg(offset);

	size_t bytesToRead = lf->keys.size() * (sizeof(lf->keys[0]));
	inputfile.read((char*)(&lf->keys[0]), bytesToRead);

	//read text filepointers
	inputfile.read((char*)(&lf->textPointers[0]), (lf->textPointers.size() * sizeof(long)));

	//read next pointer
	inputfile.read((char*)&lf->nextLeafPointer, sizeof(long));

	inputfile.close();
	return lf;
}

void BPlusTree::splitNode(LeafNode *leaftobesplit, string key, long ptr) {
	int maxSize = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;
	//create an extra leaf node
	LeafNode* leaf = getEmptyLeafNode();
	// increase size of the leaf node to be split so that the new node can be inserted before splitting the node
	leaftobesplit->keys.resize(leaftobesplit->keys.size() + 1);
	leaftobesplit->keys.resize(leaftobesplit->textPointers.size() + 1);

	leaftobesplit->keys[leaftobesplit->keys.size()] = key;
	sort(leaftobesplit->keys.begin(), leaftobesplit->keys.end());
	int pos = (find(leaftobesplit->keys.begin(), leaftobesplit->keys.end(), key)) - leaftobesplit->keys.begin();
	leaftobesplit->textPointers.insert(leaftobesplit->textPointers.begin() + pos, ptr);

	int mid = leaftobesplit->keys.size() / 2;
	int j = 0;
	for (int i = mid - 1;i <= leaftobesplit->keys.size();i++) {
		leaf->keys[j] = leaftobesplit->keys[i];
		leaftobesplit->keys[i] = "";
		leaf->textPointers[j] = leaftobesplit->textPointers[i];
		leaftobesplit->textPointers[i] = 0;
		j++;
	}

	if (leaftobesplit->keys.size() >= maxSize) {
		leaftobesplit->keys.resize(maxSize);
		leaftobesplit->textPointers.resize(maxSize);
	}

	//add new leaf node(right to mid element) to index file
	long newLeafOffset = writingLeafToFile(leaf, 0);
	addLeafOffset(newLeafOffset);
	leaftobesplit->nextLeafPointer = newLeafOffset;

	//add the split node(left of mid element) into index file
	long leaftobesplitOffset = treeStack[treeStack.size() - 1];
	long p = writingLeafToFile(leaftobesplit, leaftobesplitOffset);

	treeStack.pop_back();

	string key_interiorNode = leaf->keys[0];
	long leftptr_interiorNode = leaftobesplitOffset;
	long rightptr_interiorNode = newLeafOffset;
	bool done=true;
	while (done) {
		if (treeStack.empty()) {         //no parent node. this is the first node
			InteriorNode *in = getEmptyInteriorNode();
			in->keys.push_back(key_interiorNode);
			in->indexPointers.push_back(leftptr_interiorNode);
			in->indexPointers.push_back(rightptr_interiorNode);
			//add to index file 
			long inoffset = writingInteriorToFile(indexFile, in, 0);
			header->rootPointer = inoffset;
			ofstream file(indexFile, ios::out | ios::binary);
			file.seekp(file.beg);
			file.write((char*)header, sizeof(header));
			file.close();

			DELETE_PTR(in);
			done = true;
		}
		else {                  //there exists a parent node
			InteriorNode *parent;
			long ptrtoParent = treeStack[treeStack.size() - 1];
			parent = returnInteriorNode(indexFile, ptrtoParent);
			treeStack.pop_back();

			if (parent->keys.size() >= maxSize)
			{
				//this means the internal node is full, and requires to get split
				// create an oversized interior node with the key and ptr in correct positions
				InteriorNode *newIn = getEmptyInteriorNode();

				parent->keys.push_back(key_interiorNode);
				sort(parent->keys.begin(), parent->keys.end());
				int pos = (find(parent->keys.begin(), parent->keys.end(), key_interiorNode)) - parent->keys.begin();
				parent->indexPointers.insert(parent->indexPointers.begin() + pos + 1, rightptr_interiorNode);

				int mid1 = parent->indexPointers.size() / 2;

				key_interiorNode = parent->keys[mid - 1];
				parent->keys[mid - 1] = "";

				int k1 = 0;
				for (int i = mid1; i < parent->keys.size(); i++)
				{
					newIn->keys[k1] = parent->keys[i];
					parent->keys[i] = "";
					k1++;
				}
				k1 = 0;
				for (int k = j; k < (parent->indexPointers).size(); k++)
				{
					newIn->indexPointers[k1] = parent->indexPointers[k];
					parent->indexPointers[k] = 0;
					k1++;
				}

				//write new interior nodr into index file
				long newinterioroffset = writingInteriorToFile(indexFile, newIn, 0);
				rightptr_interiorNode = newinterioroffset;	//function shud return offset of the interior node form index file


				DELETE_PTR(newIn);
			}
			else
			{
				//internal node is not full, and the new entry can directly be inserted in the correct posiiton
				int pos = (find(parent->keys.begin(), parent->keys.end(), "")) - parent->keys.begin();
				parent->keys[pos] = key_interiorNode;
				sort(parent->keys.begin(), parent->keys.begin() + pos);

				pos = (find(parent->keys.begin(), parent->keys.end(), key_interiorNode)) - parent->keys.begin();
				parent->indexPointers.insert(parent->indexPointers.begin() + pos + 1, rightptr_interiorNode);

				if (parent->indexPointers.size() > maxSize + 1)
					parent->indexPointers.resize(maxSize + 1);


				done = true;
			}
			DELETE_PTR(parent);
		}
	}
	DELETE_PTR(leaf);
}

void BPlusTree::insertIntoLeafNode(LeafNode *lf, string newKey, long ptr) {
	int maxSize = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;  //44 records
	if (find(lf->keys.begin(), lf->keys.end(), "") != lf->keys.end()) {          // If leaf node not full
																				 /*insert into the vector. Sort the vector and find the position at which the key has
																				 been inserted*/
		int position = find(lf->keys.begin(), lf->keys.end(), "") - (lf->keys.begin());
		lf->keys[position] = newKey;
		sort(lf->keys.begin(), lf->keys.begin() + position);
		position = find(lf->keys.begin(), lf->keys.end(), newKey) - (lf->keys.begin());
		lf->textPointers.insert(lf->textPointers.begin() + position, ptr);

		maxSize = ((1024 - sizeof(long)) / (header->keySize + sizeof(long))) - 1;
		if (lf->textPointers.size() >= maxSize)
			lf->textPointers.resize(maxSize);

		long ptrtoleaf = 0;
		//already existing leaf
		if (!treeStack.empty())
		{
			ptrtoleaf = treeStack.back();
			treeStack.pop_back();
			long writeoffset = writingLeafToFile(lf, ptrtoleaf);
			addLeafOffset(writeoffset);
		}
		//first insertion - leaf node is the root node and the leaf node is written into file for the first time.
		else if (header->rootPointer == 0)
		{
			ptrtoleaf = FIRST_RECORD_OFFSET;
			long writeoffset = writingLeafToFile(lf, ptrtoleaf);
			header->rootPointer = writeoffset;
			header->leafPointer = writeoffset;
			addLeafOffset(writeoffset);

			ofstream file(indexFile, ios::out | ios::binary);
			file.seekp(file.beg);
			file.write((char*)header, sizeof(header));
			file.close();

			treeStack.push_back(writeoffset);
		}
		//leaf node is the existing root node
		else {
			ptrtoleaf = header->rootPointer;
			long writeoffset = writingLeafToFile(lf, ptrtoleaf);
			addLeafOffset(writeoffset);
		}
	}
	else    //leaf node full
		splitNode(lf, newKey, ptr);
}

bool BPlusTree::isLeafNode(long ptr)
{
	if ((find(leafNodeOffsets.begin(), leafNodeOffsets.end(), ptr)) != leafNodeOffsets.end())
		return true;
	else
		return false;

}

long BPlusTree::getRightTreePointer(InteriorNode node, string key)
{
	int pos = (find(node.keys.begin(), node.keys.end(), key)) - node.keys.begin();
	return node.indexPointers.at(pos + 1);
}

//get the left node pointer
long BPlusTree::getLeftTreePointer(InteriorNode node, string key)
{
	int pos = (find(node.keys.begin(), node.keys.end(), key)) - node.keys.begin();
	return node.indexPointers.at(pos);
}

LeafNode* BPlusTree::search(InteriorNode* node, string key) {
	//if the key is less than the first key of the interior node, search through the left subtree

	if (key < node->keys.at(0))
	{
		long lefttreeptr = getLeftTreePointer(*node, node->keys.at(0));

		if (isLeafNode(lefttreeptr))
		{
			treeStack.push_back(lefttreeptr);
			// retrieve the leaf node    
			currentLeaf = returnLeafNode(indexFile, lefttreeptr);
			cout << currentLeaf->keys[0];
			return currentLeaf;
		}
		else {

			//retrieve InteriorNode
			InteriorNode* in;
			in = returnInteriorNode(indexFile, lefttreeptr);
			treeStack.push_back(lefttreeptr);
			return search(in, key);
		}
	}
	else if (key > node->keys.at(node->keys.size() - 1))  //if the key is greater than the last key of the interior node, search through tht right subtree
	{
		long righttreeptr = getRightTreePointer(*node, node->keys.at(node->keys.size() - 1));
		if (isLeafNode(righttreeptr))
		{
			treeStack.push_back(righttreeptr);
			//retrieve the leaf node    
			currentLeaf = returnLeafNode(indexFile, righttreeptr);
			cout << currentLeaf->keys[0];
			return currentLeaf;
		}
		else {
			//retrieve InteriorNode
			InteriorNode* in;
			in = returnInteriorNode(indexFile, righttreeptr);
			treeStack.push_back(righttreeptr);
			return search(in, key);
		}
	}
	else {     //if the key is >= previous key and < the next key, search through the middle subtrees
		for (int i = 1; i <= node->keys.size() - 2; i++)
		{
			if ((key >= node->keys.at(i)) && key < node->keys.at(i + 1)) //if key found
			{
				long righttreeptr = getRightTreePointer(*node, node->keys.at(i));
				if (isLeafNode(righttreeptr))
				{
					treeStack.push_back(righttreeptr);
					//retrieve the leaf node   
					currentLeaf = returnLeafNode(indexFile, righttreeptr);
					cout << currentLeaf->keys[0];
					return currentLeaf;
				}
				else {
					//retrieve InteriorNode
					InteriorNode* in;
					in = returnInteriorNode(indexFile, righttreeptr);
					treeStack.push_back(righttreeptr);
					return search(in, key);
				}
			}

		}
	}
	cout << currentLeaf->keys[0];
	return currentLeaf;
}

void BPlusTree::insertInteriorNode(InteriorNode* in, string key, long ptr) {
	treeStack.empty();
	LeafNode* leafnode = search(in, key);
	insertIntoLeafNode(leafnode, key, ptr);
}

BPlusTree *btree;

void insertNewNode(string newKey, long newPtr) {
	if (btree->header->rootPointer == 0) {   //if first element of the b+ tree
		LeafNode *leafNode = btree->getEmptyLeafNode();
		btree->insertIntoLeafNode(leafNode, newKey, newPtr);
		DELETE_PTR(leafNode);
	}
	else {
		if (btree->header->rootPointer == btree->header->leafPointer) {    //root is a leaf node
			LeafNode *root = btree->returnLeafNode(btree->indexFile, btree->header->rootPointer);
			btree->insertIntoLeafNode(root, newKey, newPtr);
		}
		else {   //interior node
			InteriorNode *in = btree->returnInteriorNode(btree->indexFile, btree->header->rootPointer);
			btree->insertInteriorNode(in, newKey, newPtr);
		}
	}
}

void createIndex(char *dataFileName, char *indexFileName, int keyLength) {
	//Initialising B+ tree components

	btree->header = new Header();
	strcpy_s(btree->header->dataFile, dataFileName);
	btree->header->keySize = keyLength;
	btree->indexFile = indexFileName;
	btree->header->rootPointer = 0;
	btree->header->leafPointer = 55555;

	//write header data to index file
	ofstream file(btree->indexFile, ios::out | ios::binary);
	file.seekp(file.beg);
	file.write((char*)btree->header, sizeof(btree->header));
	file.close();

	//open dataFile
	ifstream inputDataFile;
	inputDataFile.open(dataFileName, ios::in);
	if (!inputDataFile.is_open()) {
		cout << "could not open the data file";
		return;
	}
	
	long pointerToNextRecord = 0;
	string inputTextline;
	for (int i = 0;i<=100;i++) {
		getline(inputDataFile, inputTextline);
		string key = inputTextline.substr(0, keyLength);
		//inserting key and index/data pointer pair
		cout << "inserting"<<key;
		insertNewNode(key, pointerToNextRecord);
		pointerToNextRecord += (long)inputTextline.length();
	}
}

int main(int argc, char* argv) {
	btree = new BPlusTree();
	int keysize;
	cout << "Enter key length";
	cin >> keysize;
	cout << "creating index file";
	createIndex("TestData.txt", "index.idx", keysize);
	delete btree;
	return 0;
}
