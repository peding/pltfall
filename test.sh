(for f in example/*.obf
do
	echo -e "\n------ testing:" $f "------"
	./$f

	echo -e "\n--- ltrace output ---"
	ltrace ./$f

	echo -e "\n------ testing:" $f " strncmp first ------"
	./$f 1

	echo -e "\n--- ltrace output ---"
	ltrace ./$f 1

done) 2>&1 | tee test.txt
