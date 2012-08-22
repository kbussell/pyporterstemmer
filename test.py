from PorterStemmer import stem, set_stopwords

def test(word):
    print "%s -> %s" % (word, stem(word))

test(u'whipped')
test(u'whipping')
test(u'halves')
set_stopwords([u'whipped'])
test(u'whipped')
test(u'whipping')
test(u'halves')
set_stopwords([u'whipped'])
print stem(u'whipped')
print stem(u'whipping')
set_stopwords([u'whipped', u'whipping'])
print stem(u'whipped')
print stem(u'whipping')
