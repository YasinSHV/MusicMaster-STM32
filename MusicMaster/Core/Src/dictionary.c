/*
 * dictionary.c
 *
 *  Created on: May 4, 2024
 *      Author: YasinShabaniVaraki
 */

#include "dictionary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define your dictionary node
struct DictionaryNode {
	char *stringKey;
	struct Tone *tones;
	int numTones;
	int black_listed; // Flag indicating if the node is blacklisted(for shuffle)
	struct DictionaryNode *next;
};

// Define your dictionary
struct Dictionary {
	struct DictionaryNode **buckets;
	int size;
};

// Hash function for string keys
unsigned int hashString(const char *key, int size) {
	unsigned int hash = 0;
	while (*key) {
		hash = (hash << 5) + *key++;
	}
	return hash % size;
}

// Initialize dictionary
Dictionary* initDictionary(int size) {
	Dictionary *dict = malloc(sizeof(Dictionary));
	dict->buckets = malloc(size * sizeof(struct DictionaryNode*));
	dict->size = size;
	for (int i = 0; i < size; i++) {
		dict->buckets[i] = NULL;
	}
	return dict;
}

// Insert key-value pair into dictionary
void insert(Dictionary *dict, const char *stringKey, struct Tone *tones,
		int numTones) {
	unsigned int index;
	struct DictionaryNode *newNode = malloc(sizeof(struct DictionaryNode));
	newNode->stringKey = strdup(stringKey);
	newNode->tones = malloc(numTones * sizeof(struct Tone));
	memcpy(newNode->tones, tones, numTones * sizeof(struct Tone));
	newNode->numTones = numTones;
	newNode->black_listed = 0;

	if (stringKey != NULL) {
		index = hashString(stringKey, dict->size);
	}

	newNode->next = dict->buckets[index];
	dict->buckets[index] = newNode;
}

// Lookup value by key
struct Tone* lookup(Dictionary *dict, const char *stringKey, int *numTones,
		struct DictionaryNode **node) {
	unsigned int index;
	struct DictionaryNode *current;

	if (stringKey != NULL) {
		index = hashString(stringKey, dict->size);
		current = dict->buckets[index];
		while (current) {
			if (current->stringKey
					&& strcmp(current->stringKey, stringKey) == 0) {
				*numTones = current->numTones;
				*node = current;
				return current->tones;
			}
			current = current->next;
		}
	}

	*numTones = 0;
	return NULL;
}

int getDictSize(Dictionary *dict) {
	return dict->size;
}

// Function to set black_listed flag on
void setBlacklisted(struct DictionaryNode *node) {
    node->black_listed = 1;
}

// Function to set black_listed flag off
void unsetBlacklisted(struct DictionaryNode *node) {
    node->black_listed = 0;
}

// Function to check if a node is blacklisted
int isBlacklisted(struct DictionaryNode *node) {
    return node->black_listed;
}


// Free dictionary memory
void freeDictionary(Dictionary *dict) {
	for (int i = 0; i < dict->size; i++) {
		struct DictionaryNode *current = dict->buckets[i];
		while (current) {
			struct DictionaryNode *temp = current;
			current = current->next;
			free(temp->stringKey);
			free(temp->tones);
			free(temp);
		}
	}
	free(dict->buckets);
	free(dict);
}
