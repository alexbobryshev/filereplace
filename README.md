
** FileReplace **

A very simple C++ tool for build and pipeline automation.
It can replace macro in text file to strings or content from another files.

How to use:

filereplace infile.txt outfile.txt MACRO1=NewText MACRO2=AnotherText MACRO3=@filepath

Opens infile.txt and replace all tokens 'MACRO1' to 'NewText', 'MACRO2' to 'AnotherText', 'MACRO3' to content that will be read from file 'filepath'
