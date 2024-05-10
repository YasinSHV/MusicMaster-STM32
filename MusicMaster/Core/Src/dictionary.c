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
    int numericKey;
    struct Tone *tones;
    int numTones;
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

// Hash function for numeric keys
unsigned int hashNumeric(int key, int size) {
    return key % size;
}

// Initialize dictionary
Dictionary *initDictionary(int size) {
    Dictionary *dict = malloc(sizeof(Dictionary));
    dict->buckets = malloc(size * sizeof(struct DictionaryNode *));
    dict->size = size;
    for (int i = 0; i < size; i++) {
        dict->buckets[i] = NULL;
    }
    return dict;
}

// Insert key-value pair into dictionary
void insert(Dictionary *dict, const char *stringKey, int numericKey, struct Tone *tones, int numTones) {
    unsigned int index;
    struct DictionaryNode *newNode = malloc(sizeof(struct DictionaryNode));
    newNode->stringKey = strdup(stringKey);
    newNode->numericKey = numericKey;
    newNode->tones = malloc(numTones * sizeof(struct Tone));
    memcpy(newNode->tones, tones, numTones * sizeof(struct Tone));
    newNode->numTones = numTones;

    if (stringKey != NULL) {
        index = hashString(stringKey, dict->size);
    } else {
        index = hashNumeric(numericKey, dict->size);
    }

    newNode->next = dict->buckets[index];
    dict->buckets[index] = newNode;
}

// Lookup value by key
struct Tone *lookup(Dictionary *dict, const char *stringKey, int numericKey, int *numTones) {
    unsigned int index;
    struct DictionaryNode *current;

    if (stringKey != NULL) {
        index = hashString(stringKey, dict->size);
        current = dict->buckets[index];
        while (current) {
            if (current->stringKey && strcmp(current->stringKey, stringKey) == 0) {
                *numTones = current->numTones;
                return current->tones;
            }
            current = current->next;
        }
    } else {
        index = hashNumeric(numericKey, dict->size);
        current = dict->buckets[index];
        while (current) {
            if (current->numericKey == numericKey) {
                *numTones = current->numTones;
                return current->tones;
            }
            current = current->next;
        }
    }

    *numTones = 0;
    return NULL;
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
