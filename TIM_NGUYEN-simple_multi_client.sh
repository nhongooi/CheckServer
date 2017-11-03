#!/bin/bash

#simple
i=1
until [ $i -gt 20 ]
do	
	python spell_check_cli.py -p 8080 -s 127.0.0.1 -f test > test$i.txt  &
	((i++))
done
echo completed
