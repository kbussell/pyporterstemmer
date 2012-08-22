/*
    Original Comment:
 
    This is the Porter stemming algorithm, coded up as thread-safe ANSI C
    by the author.
    
    It may be be regarded as cononical, in that it follows the algorithm
    presented in
    
    Porter, 1980, An algorithm for suffix stripping, Program, Vol. 14,
    no. 3, pp 130-137,
    
    only differing from it at the points maked --DEPARTURE-- below.
    
    See also http://www.tartarus.org/~martin/PorterStemmer
    
    The algorithm as described in the paper could be exactly replicated
    by adjusting the points of DEPARTURE, but this is barely necessary,
    because (a) the points of DEPARTURE are definitely improvements, and
    (b) no encoding of the Porter stemmer I have seen is anything like
    as exact as this version, even with the points of DEPARTURE!
    
    You can compile it on Unix with 'gcc -O3 -o stem stem.c' after which
    'stem' takes a list of inputs and sends the stemmed equivalent to
    stdout.
    
    The algorithm as encoded here is particularly fast.
    
    Release 2 (the more old-fashioned, non-thread-safe version may be
    regarded as release 1.)

    --------------------------------------------------------------------
    
    $KB: This has been modified fairly heavily to work as a python extension.
    
    The functions have been updated to work with Py_UNICODE character types
    instead of chars. This adds complexity, as Python's Py_UNICODE may or may
    not be binary compatable with wchar_t. See http://docs.python.org/api/unicodeObjects.html
    for more info.
    
    To be portable, all static strings need to be generated as arrays of
    Py_UNICODE. This can be seen by the abundance of lines like:
    
    static const Py_UNICODE step4_al[] = {2, 'a','l', '\0'};

*/

#include <Python.h>  /* -PY- */
#include <stdlib.h>  /* for malloc, free */
#include <string.h>  /* for memcmp, memmove */
#include <set>

/*  You will probably want to move the following declarations to a central
    header file.
*/

#define __U__ (Py_UNICODE)

size_t pyunicode_slen(const Py_UNICODE* p_str)
{
    const Py_UNICODE* p_end = p_str;
    while(*p_end++);
    return (p_end - p_str - 1);
}

int pyunicode_strcmp (const Py_UNICODE* p_src, const Py_UNICODE* p_dst)
{
    int ret = 0;

    while( ! (ret = *p_src - *p_dst) && *p_dst)
        ++p_src, ++p_dst;

    if ( ret < 0 )
        ret = -1;
    else if ( ret > 0 )
        ret = 1;

    return( ret );
}

void pyunicode_print(const Py_UNICODE* p_str)
{
    const Py_UNICODE* p_end = p_str;
    while(*p_end)
    {
        printf("%c", (char)*p_end);
        p_end++;
    }
}

struct stemmer;

struct lesswstr
{
    bool operator()(const Py_UNICODE* s1, const Py_UNICODE* s2) const
    {
        return pyunicode_strcmp(s1, s2) < 0;
    }
};

typedef std::set<const Py_UNICODE*, lesswstr> StopwordSet;
static StopwordSet g_stopwords;

extern struct stemmer * create_stemmer(void);
extern void free_stemmer(struct stemmer * z);

extern int stem(struct stemmer * z, Py_UNICODE * b, int k);


/* The main part of the stemming algorithm starts here.
*/

#define TRUE 1
#define FALSE 0

/* stemmer is a structure for a few local bits of data,
*/

struct stemmer {
    Py_UNICODE * b;    /* buffer for word to be stemmed */
    int k;          /* offset to the end of the string */
    int j;          /* a general offset into the string */
};


/*  Member b is a buffer holding a word to be stemmed. The letters are in
    b[0], b[1] ... ending at b[z->k]. Member k is readjusted downwards as
    the stemming progresses. Zero termination is not in fact used in the
    algorithm.

    Note that only lower case sequences are stemmed. Forcing to lower case
    should be done before stem(...) is called.


    Typical usage is:

        struct stemmer * z = create_stemmer();
        Py_UNICODE b[] = "pencils";
        int res = stem(z, b, 6);
            /- stem the 7 characters of b[0] to b[6]. The result, res,
               will be 5 (the 's' is removed). -/
        free_stemmer(z);
*/


extern struct stemmer * create_stemmer(void)
{
    return new stemmer;
    /* assume malloc succeeds */
}

extern void free_stemmer(stemmer * z)
{
    delete z;
}


/*  cons(z, i) is TRUE <=> b[i] is a consonant. ('b' means 'z->b', but here
    and below we drop 'z->' in comments.
*/

static int cons(struct stemmer * z, int i)
{
    switch (z->b[i])
    {
        case __U__'a': case __U__'e': case __U__'i': case __U__'o': case __U__'u': return FALSE;
        case __U__'y': return (i == 0) ? TRUE : !cons(z, i - 1);
        default: return TRUE;
    }
}

/*  m(z) measures the number of consonant sequences between 0 and j. if c is
    a consonant sequence and v a vowel sequence, and <..> indicates arbitrary
    presence,

        <c><v>       gives 0
        <c>vc<v>     gives 1
        <c>vcvc<v>   gives 2
        <c>vcvcvc<v> gives 3
        ....
*/

static int m(struct stemmer * z)
{  
    int n = 0;
    int i = 0;
    int j = z->j;
    while(TRUE)
    {
        if (i > j) return n;
        if (! cons(z, i)) break; i++;
    }
    i++;
    while(TRUE)
    {  
        while(TRUE)
        {  
            if (i > j) return n;
            if (cons(z, i)) break;
            i++;
        }
        i++;
        n++;
        while(TRUE)
        {
            if (i > j) return n;
            if (! cons(z, i)) break;
            i++;
        }
        i++;
    }
}

/* vowelinstem(z) is TRUE <=> 0,...j contains a vowel */

static int vowelinstem(struct stemmer * z)
{
    int j = z->j;
    int i; for (i = 0; i <= j; i++) if (! cons(z, i)) return TRUE;
    return FALSE;
}

/* doublec(z, j) is TRUE <=> j,(j-1) contain a double consonant. */

static int doublec(struct stemmer * z, int j)
{
    Py_UNICODE * b = z->b;
    if (j < 1) return FALSE;
    if (b[j] != b[j - 1]) return FALSE;
    return cons(z, j);
}

/*  cvc(z, i) is TRUE <=> i-2,i-1,i has the form consonant - vowel - consonant
    and also if the second c is not w,x or y. this is used when trying to
    restore an e at the end of a short word. e.g.

        cav(e), lov(e), hop(e), crim(e), but
        snow, box, tray.

*/

static int cvc(struct stemmer * z, int i)
{  
    if (i < 2 || !cons(z, i) || cons(z, i - 1) || !cons(z, i - 2)) return FALSE;
    {
        int ch = z->b[i];
        if (ch  == __U__'w' || ch == __U__'x' || ch == __U__'y') return FALSE;
    }
    return TRUE;
}

/* ends(z, s) is TRUE <=> 0,...k ends with the string s. */

static int ends(struct stemmer * z, const Py_UNICODE * s)
{  
    int length = s[0];
    Py_UNICODE * b = z->b;
    int k = z->k;
    if (s[length] != b[k]) return FALSE; /* tiny speed-up */
    if (length > k + 1) return FALSE;
    if (memcmp(b + k - length + 1, s + 1, length * sizeof(Py_UNICODE)) != 0) return FALSE;
    z->j = k-length;
    return TRUE;
}

/* setto(z, s) sets (j+1),...k to the characters in the string s, readjusting
    k. */

static void setto(struct stemmer * z, const Py_UNICODE * s)
{  
    int length = s[0];
    int j = z->j;
    memmove(z->b + j + 1, s + 1, length * sizeof(Py_UNICODE));
    z->k = j+length;
}

/* r(z, s) is used further down. */

static void r(struct stemmer * z, const Py_UNICODE * s) { if (m(z) > 0) setto(z, s); }

/*  $KB: splitting step1ab into two functions--one to deal with pluralization,
    the other for the rest. This is a stop-gap measure before handling word
    forms in a generic way. */

/* step1a(z) gets rid of plurals e.g.

        caresses  ->  caress
        ponies    ->  poni
        ties      ->  ti
        caress    ->  caress
        cats      ->  cat
        meetings  ->  meeting
*/


unsigned short a[] = {'t', 'e', 's', 't'};

static const Py_UNICODE step1a_sses[] = {4, 's', 's', 'e', 's', '\0'};
static const Py_UNICODE step1a_ies[]  = {3, 'i', 'e', 's', '\0'};
static const Py_UNICODE step1a_i[]    = {1, 'i', '\0'};

static void step1a(struct stemmer * z)
{
    Py_UNICODE * b = z->b;
    if (b[z->k] == __U__'s')
    {
        if (ends(z, step1a_sses)) z->k -= 2; else
        if (ends(z, step1a_ies)) setto(z, step1a_i); else
        if (b[z->k - 1] != __U__'s') z->k--;
    }
}

/* step1b(z) gets rid of -ed or -ing. e.g.

        feed      ->  feed
        agreed    ->  agree
        disabled  ->  disable

        matting   ->  mat
        mating    ->  mate
        meeting   ->  meet
        milling   ->  mill
        messing   ->  mess
*/

static const Py_UNICODE step1b_eed[]  = {3, 'e', 'e', 'd', '\0'};
static const Py_UNICODE step1b_ed[]   = {2, 'e', 'd', '\0'};
static const Py_UNICODE step1b_ing[]  = {3, 'i', 'n', 'g', '\0'};
static const Py_UNICODE step1b_at[]   = {2, 'a', 't', '\0'};
static const Py_UNICODE step1b_ate[]  = {3, 'a', 't', 'e', '\0'};
static const Py_UNICODE step1b_bl[]   = {2, 'b', 'l', '\0'};
static const Py_UNICODE step1b_ble[]  = {3, 'b', 'l', 'e', '\0'};
static const Py_UNICODE step1b_iz[]   = {2, 'i', 'z', '\0'};
static const Py_UNICODE step1b_ize[]  = {3, 'i', 'z', 'e', '\0'};
static const Py_UNICODE step1b_e[]    = {1, 'e', '\0'};

static void step1b(struct stemmer * z)
{
    Py_UNICODE * b = z->b;
    if (ends(z, step1b_eed)) { if (m(z) > 0) z->k--; } else
    if ((ends(z, step1b_ed) || ends(z, step1b_ing)) && vowelinstem(z))
    {
        z->k = z->j;
        if (ends(z, step1b_at)) setto(z, step1b_ate); else
        if (ends(z, step1b_bl)) setto(z, step1b_ble); else
        if (ends(z, step1b_iz)) setto(z, step1b_ize); else
        if (doublec(z, z->k))
        {
            z->k--;
            {
                int ch = b[z->k];
                if (ch == __U__'l' || ch == __U__'s' || ch == __U__'z') z->k++;
            }
        }
        else if (m(z) == 1 && cvc(z, z->k)) setto(z, step1b_e);
    }
}

/* step1c(z) turns terminal y to i when there is another vowel in the stem. */

static const Py_UNICODE step1c_y[]    = {1, 'y', '\0'};

static void step1c(struct stemmer * z)
{
    if (ends(z, step1c_y) && vowelinstem(z)) z->b[z->k] = __U__'i';
}


/*  step2(z) maps double suffices to single ones. so -ization ( = -ize plus
    -ation) maps to -ize etc. note that the string before the suffix must give
    m(z) > 0. */

static const Py_UNICODE step2_ational[] = {7, 'a','t','i','o','n','a','l', '\0'};
static const Py_UNICODE step2_ate[] = {3, 'a','t','e', '\0'};
static const Py_UNICODE step2_tional[] = {6, 't','i','o','n','a','l', '\0'};
static const Py_UNICODE step2_tion[] = {4, 't','i','o','n', '\0'};
static const Py_UNICODE step2_enci[] = {4, 'e','n','c','i', '\0'};
static const Py_UNICODE step2_ence[] = {4, 'e','n','c','e', '\0'};
static const Py_UNICODE step2_anci[] = {4, 'a','n','c','i', '\0'};
static const Py_UNICODE step2_ance[] = {4, 'a','n','c','e', '\0'};
static const Py_UNICODE step2_izer[] = {4, 'i','z','e','r', '\0'};
static const Py_UNICODE step2_ize[] = {3, 'i','z','e', '\0'};
static const Py_UNICODE step2_bli[] = {3, 'b','l','i', '\0'};
static const Py_UNICODE step2_ble[] = {3, 'b','l','e', '\0'};
static const Py_UNICODE step2_abli[] = {4, 'a','b','l','i', '\0'};
static const Py_UNICODE step2_able[] = {4, 'a','b','l','e', '\0'};
static const Py_UNICODE step2_alli[] = {4, 'a','l','l','i', '\0'};
static const Py_UNICODE step2_al[] = {2, 'a','l', '\0'};
static const Py_UNICODE step2_entli[] = {5, 'e','n','t','l','i', '\0'};
static const Py_UNICODE step2_ent[] = {3, 'e','n','t', '\0'};
static const Py_UNICODE step2_eli[] = {3, 'e','l','i', '\0'};
static const Py_UNICODE step2_e[] = {1, 'e', '\0'};
static const Py_UNICODE step2_ousli[] = {5, 'o','u','s','l','i', '\0'};
static const Py_UNICODE step2_ous[] = {3, 'o','u','s', '\0'};
static const Py_UNICODE step2_ization[] = {7, 'i','z','a','t','i','o','n', '\0'};
static const Py_UNICODE step2_ation[] = {5, 'a','t','i','o','n', '\0'};
static const Py_UNICODE step2_ator[] = {4, 'a','t','o','r', '\0'};
static const Py_UNICODE step2_alism[] = {5, 'a','l','i','s','m', '\0'};
static const Py_UNICODE step2_iveness[] = {7, 'i','v','e','n','e','s','s', '\0'};
static const Py_UNICODE step2_ive[] = {3, 'i','v','e', '\0'};
static const Py_UNICODE step2_fulness[] = {7, 'f','u','l','n','e','s','s', '\0'};
static const Py_UNICODE step2_ful[] = {3, 'f','u','l', '\0'};
static const Py_UNICODE step2_ousness[] = {7, 'o','u','s','n','e','s','s', '\0'};
static const Py_UNICODE step2_aliti[] = {5, 'a','l','i','t','i', '\0'};
static const Py_UNICODE step2_iviti[] = {5, 'i','v','i','t','i', '\0'};
static const Py_UNICODE step2_biliti[] = {6, 'b','i','l','i','t','i', '\0'};
static const Py_UNICODE step2_logi[] = {4, 'l','o','g','i', '\0'};
static const Py_UNICODE step2_log[] = {3, 'l','o','g', '\0'};

static void step2(struct stemmer * z)
{ 
    switch (z->b[z->k-1])
    {
    case __U__'a': 
        if (ends(z, step2_ational)) { r(z, step2_ate); break; }
        if (ends(z, step2_tional)) { r(z, step2_tion); break; }
        break;
    case __U__'c': 
        if (ends(z, step2_enci)) { r(z, step2_ence); break; }
        if (ends(z, step2_anci)) { r(z, step2_ance); break; }
        break;
    case __U__'e':
        if (ends(z, step2_izer)) { r(z, step2_ize); break; }
        break;
    case __U__'l':
        if (ends(z, step2_bli)) { r(z, step2_ble); break; } /*-DEPARTURE-*/

 /* To match the published algorithm, replace this line with
    case __U__'l': if (ends(z, step2_abli)) { r(z, step2_able); break; } */

        if (ends(z, step2_alli)) { r(z, step2_al); break; }
        if (ends(z, step2_entli)) { r(z, step2_ent); break; }
        if (ends(z, step2_eli)) { r(z, step2_e); break; }
        if (ends(z, step2_ousli)) { r(z, step2_ous); break; }
        break;
    case __U__'o':
        if (ends(z, step2_ization)) { r(z, step2_ize); break; }
        if (ends(z, step2_ation)) { r(z, step2_ate); break; }
        if (ends(z, step2_ator)) { r(z, step2_ate); break; }
        break;
    case __U__'s':
        if (ends(z, step2_alism)) { r(z, step2_al); break; }
        if (ends(z, step2_iveness)) { r(z, step2_ive); break; }
        if (ends(z, step2_fulness)) { r(z, step2_ful); break; }
        if (ends(z, step2_ousness)) { r(z, step2_ous); break; }
        break;
    case __U__'t':
        if (ends(z, step2_aliti)) { r(z, step2_al); break; }
        if (ends(z, step2_iviti)) { r(z, step2_ive); break; }
        if (ends(z, step2_biliti)) { r(z, step2_ble); break; }
        break;
    case __U__'g':
        if (ends(z, step2_logi)) { r(z, step2_log); break; } /*-DEPARTURE-*/
    }
}

/* step3(z) deals with -ic-, -full, -ness etc. similar strategy to step2. */

static const Py_UNICODE step3_icate[] = {5, 'i','c','a','t','e', '\0'};
static const Py_UNICODE step3_ic[] = {2, 'i','c', '\0'};
static const Py_UNICODE step3_ative[] = {5, 'a','t','i','v','e', '\0'};
static const Py_UNICODE step3_null[] = {0, '\0'};
static const Py_UNICODE step3_alize[] = {5, 'a','l','i','z','e', '\0'};
static const Py_UNICODE step3_al[] = {2, 'a','l', '\0'};
static const Py_UNICODE step3_iciti[] = {5, 'i','c','i','t','i', '\0'};
static const Py_UNICODE step3_ical[] = {4, 'i','c','a','l', '\0'};
static const Py_UNICODE step3_ful[] = {3, 'f','u','l', '\0'};
static const Py_UNICODE step3_ness[] = {4, 'n','e','s','s', '\0'};

static void step3(struct stemmer * z) 
{ 
    switch (z->b[z->k])
    {
    case L'e':
        if (ends(z, step3_icate)) { r(z, step3_ic); break; }
        if (ends(z, step3_ative)) { r(z, step3_null); break; }
        if (ends(z, step3_alize)) { r(z, step3_al); break; }
        break;
    case L'i':
        if (ends(z, step3_iciti)) { r(z, step3_ic); break; }
        break;
    case L'l':
        if (ends(z, step3_ical)) { r(z, step3_ic); break; }
        if (ends(z, step3_ful)) { r(z, step3_null); break; }
        break;
    case L's':
        if (ends(z, step3_ness)) { r(z, step3_null); break; }
        break;
    }
}

/* step4(z) takes off -ant, -ence etc., in context <c>vcvc<v>. */

static const Py_UNICODE step4_al[] = {2, 'a','l', '\0'};
static const Py_UNICODE step4_ance[] = {4, 'a','n','c','e', '\0'};
static const Py_UNICODE step4_ence[] = {4, 'e','n','c','e', '\0'};
static const Py_UNICODE step4_er[] = {2, 'e','r', '\0'};
static const Py_UNICODE step4_ic[] = {2, 'i','c', '\0'};
static const Py_UNICODE step4_able[] = {4, 'a','b','l','e', '\0'};
static const Py_UNICODE step4_ible[] = {4, 'i','b','l','e', '\0'};
static const Py_UNICODE step4_ant[] = {3, 'a','n','t', '\0'};
static const Py_UNICODE step4_ement[] = {5, 'e','m','e','n','t', '\0'};
static const Py_UNICODE step4_ment[] = {4, 'm','e','n','t', '\0'};
static const Py_UNICODE step4_ent[] = {3, 'e','n','t', '\0'};
static const Py_UNICODE step4_ion[] = {3, 'i','o','n', '\0'};
static const Py_UNICODE step4_ou[] = {2, 'o','u', '\0'};
static const Py_UNICODE step4_ism[] = {3, 'i','s','m', '\0'};
static const Py_UNICODE step4_ate[] = {3, 'a','t','e', '\0'};
static const Py_UNICODE step4_iti[] = {3, 'i','t','i', '\0'};
static const Py_UNICODE step4_ous[] = {3, 'o','u','s', '\0'};
static const Py_UNICODE step4_ive[] = {3, 'i','v','e', '\0'};
static const Py_UNICODE step4_ize[] = {3, 'i','z','e', '\0'};

static void step4(struct stemmer * z)
{
    switch (z->b[z->k-1])
    {
        case __U__'a': 
            if (ends(z, step4_al)) break; return;
        case __U__'c': 
            if (ends(z, step4_ance)) break;
            if (ends(z, step4_ence)) break; return;
        case __U__'e': 
            if (ends(z, step4_er)) break; return;
        case __U__'i': 
            if (ends(z, step4_ic)) break; return;
        case __U__'l': 
            if (ends(z, step4_able)) break;
            if (ends(z, step4_ible)) break; return;
        case __U__'n': 
            if (ends(z, step4_ant)) break;
            if (ends(z, step4_ement)) break;
            if (ends(z, step4_ment)) break;
            if (ends(z, step4_ent)) break; return;
        case __U__'o': 
            if (ends(z, step4_ion) && (z->b[z->j] == 's' || z->b[z->j] == 't')) break;
            if (ends(z, step4_ou)) break; return;
            /* takes care of -ous */
        case __U__'s': 
            if (ends(z, step4_ism)) break; return;
        case __U__'t': 
            if (ends(z, step4_ate)) break;
            if (ends(z, step4_iti)) break; return;
        case __U__'u': 
            if (ends(z, step4_ous)) break; return;
        case __U__'v': 
            if (ends(z, step4_ive)) break; return;
        case __U__'z': 
            if (ends(z, step4_ize)) break; return;
        default: 
            return;
    }
    if (m(z) > 1) z->k = z->j;
}

/* step5(z) removes a final -e if m(z) > 1, and changes -ll to -l if
    m(z) > 1. */

static void step5(struct stemmer * z)
{
    Py_UNICODE * b = z->b;
    z->j = z->k;
    if (b[z->k] == __U__'e')
    {
        int a = m(z);
        if (((a > 1) || (a == 1)) && !cvc(z, z->k - 1)) z->k--;
    }
    if (b[z->k] == __U__'l' && doublec(z, z->k) && m(z) > 1) z->k--;
}

/* In stem(z, b, k), b is a Py_UNICODE pointer, and the string to be stemmed is
    from b[0] to b[k] inclusive.  Possibly b[k+1] == '\0', but it is not
    important. The stemmer adjusts the characters b[0] ... b[k] and returns
    the new end-point of the string, k'. Stemming never increases word
    length, so 0 <= k' <= k.
*/
// $KB: updated to take and return string length instead of a zero-based offset
extern int stem(struct stemmer * z, Py_UNICODE * b, int b_len, int plurals_only)
{
    if (b_len <= 2) return b_len; /*-DEPARTURE-*/
    z->b = b; z->k = (b_len-1); /* copy the parameters into z */

    /* With this line, strings of length 1 or 2 don't go through the
      stemming process, although no mention is made of this in the
      published algorithm. Remove the line to match the published
      algorithm. */

    step1a(z);
    if (plurals_only)
    {
        step5(z); // remove the trailing e
    }
    else
    {
        step1b(z); step1c(z); step2(z); step3(z); step4(z); step5(z);
    }
    return z->k + 1;
}

/* -PY- */
void dump_stopwords()
{
    printf("[");
    StopwordSet::iterator it = g_stopwords.begin();
    StopwordSet::iterator it_end = g_stopwords.end();
    for(int first_time=1; it != it_end; ++it)
    {
        if (first_time)
            first_time=0;
        else
            printf(", ");
        
        printf("'");
        pyunicode_print(*it);
        printf("'");
    }
    printf("]");
}

static PyObject* py_stem(PyObject* self, PyObject* args)
{
    const Py_UNICODE* str;
    int plurals_only = 0;

    if (!PyArg_ParseTuple(args, "u|i", &str, &plurals_only))
        return NULL;

    int str_len = pyunicode_slen(str);
    if ( str_len >= 255 )
    {
        PyErr_SetString(PyExc_IndexError, "stemmer only works with strings < 255 chars");
        return 0;
    }

/*
    printf("stopwords:");
    dump_stopwords();
    printf("\n");
*/

    PyObject* token = NULL;
    if (g_stopwords.find(str) == g_stopwords.end())
    {
        stemmer z;
    
        Py_UNICODE newstr[255] = {0};
        memcpy(newstr, str, str_len * sizeof(Py_UNICODE));
        int stem_len = stem(&z, newstr, str_len, plurals_only);
        newstr[stem_len + 1] = 0;
    
        token = PyUnicode_FromUnicode(newstr, stem_len);        
    }
    else
    {
        token = PyUnicode_FromUnicode(str, str_len);
    }

    return token;
}

static void clean_up(StopwordSet stopwords)
{
    StopwordSet::iterator it = stopwords.begin();
    StopwordSet::iterator it_end = stopwords.end();
    while((it = stopwords.begin()) != stopwords.end())
    {
        const Py_UNICODE* tmp = *it;
        stopwords.erase(it);
        delete [] tmp;
    }
}

static PyObject* py_set_stopwords(PyObject* self, PyObject* args)
{
    PyObject * p_list_obj; /* the list of strings */

    if (! PyArg_ParseTuple( args, "O!", &PyList_Type, &p_list_obj ))
        return NULL;
    
    int num_lines = PyList_Size(p_list_obj);
    if (num_lines < 0)
        return NULL; /* Not a list */

    StopwordSet stopwords;
    PyObject* p_str_obj;
    Py_UNICODE* p_stopword;
    int err = 0;
    for ( int idx = 0; idx < num_lines; ++idx )
    {
        p_str_obj = PyList_GetItem(p_list_obj, idx);
        if (PyUnicode_Check(p_str_obj) == 0)
        {
            err = 1;
            break;
        }
        Py_ssize_t len = PyUnicode_GetSize(p_str_obj);
        p_stopword = new Py_UNICODE[len+1];
        memcpy(p_stopword, ((PyUnicodeObject*)p_str_obj)->str, len * sizeof(Py_UNICODE));
        p_stopword[len] = 0;        
        stopwords.insert(p_stopword);
    }
    
    if (err)
    {
        clean_up(stopwords);
        return NULL;
    }
    
    StopwordSet old_stopwords = g_stopwords;
    g_stopwords = stopwords;
    clean_up(old_stopwords);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef StemMethods[] =
{
     {"stem", py_stem, METH_VARARGS, "run a unicode string through the Porter Stemmer."},
     {"set_stopwords", py_set_stopwords, METH_VARARGS, "assign a sequence of words for which stemming will be ignored."},
     {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initPorterStemmer(void)
{
    (void) Py_InitModule("PorterStemmer", StemMethods);
}
