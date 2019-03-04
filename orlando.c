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

/*
	What's are we doing today, Brain?

	Objective:
	By learning from a specialised corpus of input text, we can start to
	predict what the next word in a sentence (for this type of text) might be.
	This is useful for word-prediction on mobile devices.
	We could also create a sentence/text generator, see if it makes sense ;-)

	Why:
	It's a neat exercise in linguistics, maths and learning algorithms.
	Contrary to many learning systems which operate in an opaque manner,
	the specialised approach here can be easily followed and verified.

	How:
	Words and punctuation are mapped to a dynamic token dictionary.
	(we try to identify things like initials, email and URLs as tokens)
	We create a frequency network based on trigrams (A,B,C) of tokens:
	1. STX STX The
	2. STX The quick
	3. The quick brown
	4. quick brown fox
	In practice, for each token(A,B), we track the frequency of token(C).
	Then, the RELATIVE frequency of ALL occurring token(C) after token(A,B)
	allows us to derive the probability of ANY ONE token(C) following (A,B).

	We could visualise this in 3D token matrix of size N^3, however,
	let's not waste daft amounts of memory: it'll be a sparse matrix.
	So we set up a different structure. For now, we'll use a token hash array
	+ a binary tree for the trigrams with the token(C) part in a linked list.

	Ref:
	https://en.wikipedia.org/wiki/Dynamic_Bayesian_network
	https://en.wikipedia.org/wiki/Trigram
	https://en.wikipedia.org/wiki/Sparse_matrix
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



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
	struct tokenC_item		*tokenC;
};

struct token_hash_item {
	__uint16_t	*s;
	__uint16_t	freq;
}


struct token_hash_item *token_hash_table = NULL;
__uint32_t num_tokens = 0;
struct token_tree item *token_tree = NULL;

struct tokenAB_item tokenAB_state;



void init_tokendict (void)
{
	// allocate and init hash table
	token_hash_table = (token_hash_item *) calloc(MAX_TOKENS, sizeof (struct token_hash_item));
	num_tokens = 0;

	token_tree = NULL;

	init_tokenAB_state();
}/*init_tokendict()*/



void init_tokenAB_state (void)
{
	__uint16_t h = find_add_token("\002");	// STX (start of text)

	tokenAB_state.tokenA = tokenAB_state.tokenB = h;
}/*init_tokenAB_state()*/



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
		h += MAX_TOKENS;
	while (h != h2);

	// Dictionary full!
	fprintf(stderr, "Dictionary full!\n");
	exit (1);
}/*find_add_token()*/



// Add a token into the token tree
void add_token (const char *s)
{
	token_tree_item **ttipp;
	int h = find_add_token(s);

	printf("[#%05u] %s\n", h, s);

	// traverse token(A,B) tree
	ttipp = &token_tree;
	while (*ttipp != NULL) {
		if (!(i = memcmp(&((*ttipp)->tokenAB), &tokenAB_state, sizeof (struct tokenAB_item)))) {
			struct tokenC_list_item **tclipp;

			// found token(A,B), now traverse the local token(C) item list
			for (tclipp = &((*ttipp)->tokenC; *tclipp != NULL; tclipp = (*tclipp)->next) {
				if ((*tclipp)->tokenC == h) {
					// Found! Increment freq.
					if ((*tclipp)->freq == 0xffff) {
						// We'd wrap! Need to adjust the relative frequencies first.
						struct tokenC_list_item *tclip;

						(*ttipp)->tokenAB.freq = 0;
						for (tclip = (*ttipp)->tokenC; tclip; tclip = tclip->next) {
							// Halve everything!
							// Yes, this means that some end up at 0, that's ok.
							tclip->freq >>= 1;

							(*ttipp)->tokenAB.freq += tclip->freq;	// tally into token(A,B) freq.
						}
					}

					(*tclipp)->tokenC.freq++;	// now we can increment token(C) freq.
					(*ttipp)->tokenAB.freq++;	// increment token(A,B) freq as well, of course.

					// shift state B -> A, C -> B.
					tokenAB_state.tokenA = tokenAB_state.tokenB;
					tokenAB_state.tokenB = (*tclipp)->tokenC.tokenC;

					// Done!
					return;
				}
			}

			// Not found, add new token(C)
			(*tclipp)->tokenC = (struct tokenC_list_item *) calloc(1, sizeof (struct tokenC_list_item));
			(*tclipp)->tokenC.tokenC = h;
			(*tclipp)->tokenC.freq = 1;

			(*ttipp)->tokenAB.freq++;	// increment token(A,B) freq as well, of course.

			// shift state B -> A, C -> B.
			tokenAB_state.tokenA = tokenAB_state.tokenB;
			tokenAB_state.tokenB = (*tclipp)->tokenC.tokenC;

			// Done!
			return;
		}
		else {
			// Not the one... we need to go left/right on our token(A,B) tree
			ttipp = (i < 0) ? &((*ttipp)->left) : &((*ttipp)->right));
		}
	}

	// we need a new token(A,B) with our new token(C).
	*ttipp = (struct token_tree_item *) calloc(1, sizeof (struct token_tree_item));

`	(*ttipp)->tokenAB.tokenA = tokenAB_state.tokenA;
	(*ttipp)->tokenAB.tokenB = tokenAB_state.tokenB;
	(*ttipp)->tokenAB.freq = 1;

	(*ttipp)->tokenC = (struct tokenC_list_item *) calloc(1, sizeof (struct tokenC_list_item));
	(*ttipp)->tokenC.tokenC = h;
	(*ttipp)->tokenC.freq = 1;

	// shift state B -> A, C -> B.
	tokenAB_state.tokenA = tokenAB_state.tokenB;
	tokenAB_state.tokenB = (*tclipp)->tokenC.tokenC;
}/*add_token()*/



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

	exit(0);
}



/* end of orlando.c */