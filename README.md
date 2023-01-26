Orlando - because, what's in a name...


What's are we doing today, Brain?


# Objective

By learning from a specialised corpus of input text, we can start to
predict what the next word in a sentence (for this type of text) might be.

This is useful for
- word-prediction on mobile devices;
- text generator, see if it makes sense;
- assessing likelyhood of text having same author as other texts.


# Why

It's a neat exercise in linguistics, maths and learning algorithms.
Contrary to many learning systems which operate in an opaque manner,
the specialised approach here can be easily followed and verified.

And yes, modern AI/ML are "smarter", but Orlando's approach works
better than the word prediction used in most mobile phones.
Einstein noted: "Make things as simple as possible, but no simpler."
That ALSO means, don't make things more complicated than needed for its
purpose! No need to connect to the cloud for SMS word prediction.


# How

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


## Structure

We could visualise our structure in a 3D token matrix of size N^3, however,
let's not waste daft amounts of memory: it'd turn out to be a sparse matrix.
So we set up a different structure: we'll use a token hash array
 + a binary tree for the trigrams with the token(C) part in a linked list.
A neat exercise in good old common C structures, really :)


	-- Arjen Lentz, 2019


## References

- https://en.wikipedia.org/wiki/Dynamic_Bayesian_network
- https://en.wikipedia.org/wiki/Trigram
- https://en.wikipedia.org/wiki/Sparse_matrix

