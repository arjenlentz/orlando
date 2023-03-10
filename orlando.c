/*
	Orlando - word prediction
	Copyright (C) 2019 by Arjen Lentz <arjen@lentz.com.au>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

	https://www.gnu.org/licenses/agpl.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>



#define MAX_WORDLEN	1024	// long enough, but we do safety checks
#define MAX_TOKENS	65536	// fit 16-bit tokens
#define HASH_SKIP	17		// MAX_TOKENS / HASH_SKIP must not be whole



struct tokenAB_item {
	__uint16_t	tokenA,
				tokenB;
};

struct tokenC_list_item {
	__uint16_t				tokenC;
	__uint16_t				freq;	// within context of token(A,B)
	struct tokenC_list_item	*next;
};

struct token_tree_item {
	struct tokenAB_item		tokenAB;
	__uint32_t				freq;
	struct token_tree_item	*left,
							*right;
	// ---
	struct tokenC_list_item	*tokenC_listp;
};

struct token_hash_item {
	char		*s;
	__uint16_t	freq;
};


struct token_hash_item *token_hash_table = NULL;
__uint32_t num_tokens = 0;
struct token_tree_item *token_tree = NULL;

struct tokenAB_item tokenAB_state;



/*
	Simple 16-bit hash, distribution not-too-dreadful.
	For lots of hash wisdom, see:
	http://eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx
	https://en.wikipedia.org/wiki/Jenkins_hash_function
*/
__uint16_t rotxor_hash (const char *s)
{
    __uint16_t h = 0;

	while (*s) {
		// We shift by a nibble for better distribution.
		h = (h << 4) ^ (h >> 12) ^ *s++;
	}

    return (h);
}/*rotxor_hash()*/



/*
	Find a token, add if doesn't yet exist.
	If our hash table is full, things get complicated...
*/
__uint16_t find_add_token (const char *s)
{
	__uint16_t h, h2;

	h = h2 = rotxor_hash(s);
	do {
		if (!token_hash_table[h].s) {
			// Doesn't exist, so let's add it
			token_hash_table[h].s = strdup(s);
			token_hash_table[h].freq = 1;
			num_tokens++;

			return (h);
		}
		else if (!strcmp(token_hash_table[h].s, s)) {
			// Found! Increment observed frequency of this token.
			if (token_hash_table[h].freq == 0xffff) {
				// We'd wrap! Need to adjust the relative frequencies first.
				int i;

				for (i = 0; i < MAX_TOKENS; i++) {
					// Halve everything!
					// Yes, this means that some end up at 0, that's ok.
					token_hash_table[i].freq >>= 1;
				}
			}

			// Now we can increment the freq of our token.
			token_hash_table[h].freq++;

			return (h);
		}

		// Not the one, we need to skip around some more...
		// Note that we use the h var overflow to do our modulo.
		h += HASH_SKIP;
	} while (h != h2);

	// Dictionary full!
	fprintf(stderr, "Dictionary full!\n");
	exit (1);
}/*find_add_token()*/



void init_tokenAB_state (void)
{
	__uint16_t h = find_add_token("\002");	// STX (start of text)

	tokenAB_state.tokenA = tokenAB_state.tokenB = h;
}/*init_tokenAB_state()*/



void init_tokendict (void)
{
	// allocate and init hash table
	token_hash_table = (struct token_hash_item *) calloc(MAX_TOKENS, sizeof (struct token_hash_item));
	num_tokens = 0;

	token_tree = NULL;

	init_tokenAB_state();
}/*init_tokendict()*/



// Add a token into the token tree
void add_token (const char *s)
{
	struct token_tree_item **ttipp;
	int h = find_add_token(s);
	int i;

//printf("[#%05u] %s\n", h, s);

	// traverse token(A,B) tree
	ttipp = &token_tree;
	while (*ttipp != NULL) {
		if (!(i = memcmp(&((*ttipp)->tokenAB), &tokenAB_state, sizeof (struct tokenAB_item)))) {
			struct tokenC_list_item **tclipp;

			// found token(A,B), now traverse the local token(C) item list
			for (tclipp = &((*ttipp)->tokenC_listp); *tclipp != NULL; tclipp = &((*tclipp)->next)) {
				if ((*tclipp)->tokenC == h) {
					// Found! Increment freq.

					if ((*tclipp)->freq == 0xffff) {
						// We'd wrap! Need to adjust the relative frequencies first.
						struct tokenC_list_item *tclip;

						(*ttipp)->freq = 0;
						for (tclip = (*ttipp)->tokenC_listp; tclip; tclip = tclip->next) {
							// Halve everything!
							// Yes, this means that some end up at 0, that's ok.
							tclip->freq >>= 1;

							(*ttipp)->freq += tclip->freq;	// tally into token(A,B) freq.
						}
					}

//printf("[#%05u-#%05u-#%05u]++\n", tokenAB_state.tokenA, tokenAB_state.tokenB, h);
					(*tclipp)->freq++;	// now we can increment token(C) freq.
					(*ttipp)->freq++;	// increment token(A,B) freq as well, of course.

					// shift state B -> A, C -> B.
					tokenAB_state.tokenA = tokenAB_state.tokenB;
					tokenAB_state.tokenB = h;

					// Done!
					return;
				}
			}

			// Not found, add new token(C)
//printf("[#%05u-#%05u-#%05u] new(C)\n", tokenAB_state.tokenA, tokenAB_state.tokenB, h);
			(*tclipp) = (struct tokenC_list_item *) calloc(1, sizeof (struct tokenC_list_item));
			(*tclipp)->tokenC = h;
			(*tclipp)->freq = 1;

			(*ttipp)->freq++;	// increment token(A,B) freq as well, of course.

			// shift state B -> A, C -> B.
			tokenAB_state.tokenA = tokenAB_state.tokenB;
			tokenAB_state.tokenB = h;

			// Done!
			return;
		}
		else {
			// Not the one... we need to go left/right on our token(A,B) tree
			ttipp = (i < 0) ? &((*ttipp)->left) : &((*ttipp)->right);
		}
	}

	// we need a new token(A,B) with our new token(C).
	*ttipp = (struct token_tree_item *) calloc(1, sizeof (struct token_tree_item));

	(*ttipp)->tokenAB.tokenA = tokenAB_state.tokenA;
	(*ttipp)->tokenAB.tokenB = tokenAB_state.tokenB;
	(*ttipp)->freq = 1;

	(*ttipp)->tokenC_listp = (struct tokenC_list_item *) calloc(1, sizeof (struct tokenC_list_item));
	(*ttipp)->tokenC_listp->tokenC = h;
	(*ttipp)->tokenC_listp->freq = 1;

//printf("[#%05u-#%05u-#%05u] new(A,B,C)\n", tokenAB_state.tokenA, tokenAB_state.tokenB, h);
	// shift state B -> A, C -> B.
	tokenAB_state.tokenA = tokenAB_state.tokenB;
	tokenAB_state.tokenB = h;
}/*add_token()*/



// walk token tree, left first
void dump_token_tree (struct token_tree_item *ttip)
{
	struct tokenC_list_item *ttlip;

	do {
		if (ttip->left != NULL)
			dump_token_tree(ttip->left);

		printf("[#%05u,#%05u] %s %s\n",
			ttip->tokenAB.tokenA, ttip->tokenAB.tokenB,
			token_hash_table[ttip->tokenAB.tokenA].s, token_hash_table[ttip->tokenAB.tokenB].s);

		for (ttlip = ttip->tokenC_listp; ttlip != NULL; ttlip = ttlip->next) {
			printf("  [%05u] (%3.2f) %s\n",
				ttlip->tokenC,
				(float) ttlip->freq / ttip->freq,
				token_hash_table[ttlip->tokenC].s);
		}

		ttip = ttip->right;
	} while (ttip != NULL);
}/*dump_token_tree()*/



// read a stream and tokenize it
// TODO: needs to be tweaked for UTF-8 (https://github.com/chansen/c-utf8-valid)
void tokenise_stream (FILE *fp)
{
	char buf[MAX_WORDLEN + 2];
	int c, c2;
	int wordlen;

	init_tokenAB_state();

	wordlen = 0;
	while ((c = fgetc(fp)) != EOF) {
		if (strchr(" \t\n\r\v\"()[]<>", (char) c)) {
			// definite word break or newline
			// and yes, we effectively eat ()[]<>
			if (wordlen > 0) {
				buf[wordlen] = '\0';
				add_token(buf);
				wordlen = 0;
			}

			continue;
		}

		if (strchr(".,!?:;/@-_", (char) c)) {
			// if followed by a space/newline/EOF, it's a token, otherwise it might be part of a ... or URL or email address!
			c2 = fgetc(fp);
			if (c2 == EOF || isspace(c2)) {
				if (wordlen >= 2) {
					// token complete
					buf[wordlen] = '\0';
					add_token(buf);
					wordlen = 0;
					// fallthrough to 0
				}

				// 0: not in a word, so just do this char as token
				// 1: likely an initial, list or similar and we'll keep it as one token
				buf[wordlen++] = (char) c;
				buf[wordlen] = '\0';
				add_token(buf);
				wordlen = 0;

				continue;
			}

			// not a space, so stick it back
			ungetc(c2, fp);

			// drop through to regular char
		}

		// regular char, add to word
		if (wordlen < MAX_WORDLEN) {
			buf[wordlen++] = (char) c;
		}
		else {
			// prevent buffer overflow!
			fprintf(stderr, "Maximum word length (%u) exceeded!\n", MAX_WORDLEN);
			exit (1);
		}
	}

	if (wordlen > 0) {
		buf[wordlen] = '\0';
		add_token(buf);
	}

	add_token("\003");	// ETX (end of text)
}/*tokenise_stream()*/



// Try and count the actual vocabulary of the author
// We do this roughly by noting any token starting with a lowercase char
int count_vocabulary (void)
{
	int i, vocab;

	for (i = vocab = 0; i < MAX_TOKENS; i++) {
		if (token_hash_table[i].s != NULL && islower(token_hash_table[i].s[0]))
			vocab++;
	}

	return (vocab);
}/*count_vocabulary()*/



// Make up a story up to approx N words
void ghostwrite (int num_words)
{
	struct token_tree_item *ttip;
	struct tokenC_list_item *tclip;
	__uint16_t etx_token = find_add_token("\003"), fullstop_token = find_add_token(".");
	char *s;
	int i, len;

	srandom(time(NULL));
	init_tokenAB_state();

	do {
		// traverse token(A,B) tree to find current state
		for (ttip = token_tree; ttip != NULL && (i = memcmp(&(ttip->tokenAB), &tokenAB_state, sizeof (struct tokenAB_item))) != 0; ttip = (i < 0) ? ttip->left : ttip->right);

		if (ttip == NULL) {
			// Not found!? That should be impossible.
			//printf("[#%05u-#%05u]\n", tokenAB_state.tokenA, tokenAB_state.tokenB);
			fprintf(stderr, "Token(A,B) sequence not found in tree - this indicates a bug!\n");
			exit (1);
		}

		// This is our chance to be artistic, choose our next word from the possibilities
		i = random() % ttip->freq;

		for (tclip = ttip->tokenC_listp; tclip != NULL && i >= tclip->freq; i -= tclip->freq, tclip = tclip->next);

		if (tclip == NULL) {
			// Shouldn't happen.
			fprintf(stderr, "Skipped out of token(C) range - this indicates a bug!\n");
			exit (1);
		}

		s = token_hash_table[tclip->tokenC].s;
		len = strlen(s);
		if (len > 1 || isdigit(s[0]) || s[0] >= 'A') 
			printf(" ");
		printf("%s", token_hash_table[tclip->tokenC].s);
		if (len == 1 && strchr(".!?", s[0]))
			printf("\n");

		// shift state B -> A, C -> B.
		tokenAB_state.tokenA = tokenAB_state.tokenB;
		tokenAB_state.tokenB = tclip->tokenC;

		// we keep going until the word limit, and then the end of a sentence.
	} while (--num_words > 0 || (tclip->tokenC != etx_token && tclip->tokenC != fullstop_token));
}/*ghostwrite()*/



void main (int argc, char *argv[])
{
	int i;
	FILE *fp;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <infile> ...\n", argv[0]);
		exit (0);
	}

	init_tokendict();

	for (i = 1; i < argc; i++) {
		if ((fp = fopen(argv[i], "r")) == NULL) {
			fprintf(stderr, "Can't open input file '%s'\n", argv[i]);
			exit (1);
		}

		tokenise_stream(fp);
		fclose(fp);
	}

printf("num_tokens=%u  vocab=%d\n", num_tokens, count_vocabulary());

#if 0
printf("\nToken hash table:\n");
for (i = 0; i < MAX_TOKENS; i++) {
	if (token_hash_table[i].s != NULL) {
		printf("[#%05u]:%u %s\n", i, token_hash_table[i].freq, token_hash_table[i].s);
	}
}

dump_token_tree(token_tree);
#endif

	ghostwrite(500);


	exit(0);
}



/* end of orlando.c */
