// deck.h
#ifndef DECK_H
#define DECK_H

#include "common.h"

typedef uint8_t Card; // 0..51

void init_deck(Card deck[52]);
void shuffle_deck(Card deck[52], unsigned *seedp);
Card deal_card(Card deck[52], int *top_index);
void card_to_str(Card c, char *out); // out must be large enough (e.g., 4 bytes)
int hand_value(const Card *hand, int n);

#endif // DECK_H
