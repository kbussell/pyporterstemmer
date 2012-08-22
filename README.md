pyporterstemmer
===============

python C-extension implementing the Porter Stemming algorithm, modified from the C version written by Martin Porter (http://tartarus.org/~martin/PorterStemmer/)

This implementation requires input be unicode strings

Sample Usage
============

```python
>>> from PorterStemmer import stem
>>> stem(u'running')
u'run'
>>> stem(u"collaboration")
u'collabor'
```

