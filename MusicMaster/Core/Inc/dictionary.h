/*
 * dictionary.h
 *
 *  Created on: May 4, 2024
 *      Author: YasinSHV
 */

#ifndef SRC_DICTIONARY_H_
#define SRC_DICTIONARY_H_

struct Tone {
    int frequency;
    int duration;
};

struct DictionaryNode;
struct Dictionary;
typedef struct Dictionary Dictionary;


// Function declarations
Dictionary *initDictionary(int size);
void insert(Dictionary *dict, const char *stringKey, struct Tone *tones, int numTones);
struct Tone *lookup(Dictionary *dict, const char *stringKey, int *numTones, struct DictionaryNode **node);
void setBlacklisted(struct DictionaryNode *node);
void unsetBlacklisted(struct DictionaryNode *node);
int isBlacklisted(struct DictionaryNode *node);
int getDictSize();
void freeDictionary(Dictionary *dict);

#endif /* SRC_DICTIONARY_H_ */
