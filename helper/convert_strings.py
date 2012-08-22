#!/usr/bin/env python
'''
This script is no longer technically necessary, as it was used to generate
some of the code in porter_stemmer.cpp. The input came from the original C
version of the porter stemmer algorithm (check SVN history).

To use:
1) Copy and paste function into input variable
2) Replace L"\ with L"\\
3) set step_name to match the function (defined in input) name
4) save the output and paste it into the porter_stemmer.cpp function
'''

import re

str_re = re.compile(r'L"\\0(\d)" L"([^"]*)"')

input = '''
static void step3(struct stemmer * z) 
{ 
    switch (z->b[z->k])
    {
    case L'e':
        if (ends(z, L"\\05" L"icate")) { r(z, L"\\02" L"ic"); break; }
        if (ends(z, L"\\05" L"ative")) { r(z, L"\\00" L""); break; }
        if (ends(z, L"\\05" L"alize")) { r(z, L"\\02" L"al"); break; }
        break;
    case L'i':
        if (ends(z, L"\\05" L"iciti")) { r(z, L"\\02" L"ic"); break; }
        break;
    case L'l':
        if (ends(z, L"\\04" L"ical")) { r(z, L"\\02" L"ic"); break; }
        if (ends(z, L"\\03" L"ful")) { r(z, L"\\00" L""); break; }
        break;
    case L's':
        if (ends(z, L"\\04" L"ness")) { r(z, L"\\00" L""); break; }
        break;
    }
}
'''
vars = []
var_set = set()
step_name = 'step3'

def convert_str(match):
    var_suffix = match.group(2)
    if not match.group(2):
        var_suffix = 'null'
    
    var_name = '%s_%s' % (step_name, var_suffix)
    
    broken_up_string = ','.join(["'%s'" % ch for ch in match.group(2)])
    if broken_up_string:
        broken_up_string += ', '
    
    new_var = "static const Py_UNICODE %s[] = {%d, %s'\\0'};" % (var_name, int(match.group(1)), broken_up_string)
    if new_var not in var_set:
        var_set.add(new_var)
        vars.append(new_var)

    return var_name
    
output = str_re.sub(convert_str, input)
for var in vars:
    print var
print output
