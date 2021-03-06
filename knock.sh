codes=`find knock/*.ho`
tmpfile=$(mktemp)

pass=0
fail=0
for src in $codes; do
  base=$(basename $src .ho)
  testfile="knock/${base}.out"
  testin="knock/${base}.in"
  printf "$src: "
  build/ho $src < $testin 1> $tmpfile
  diff $tmpfile $testfile -u
  if [ $? = 0 ]; then
    printf "\e[32m"
    echo "PASS"
    pass=`expr $pass + 1`
  else
    printf "\e[31m"
    echo "$src: FAIL"
    fail=`expr $fail + 1`
  fi
  printf "\e[m"
done

rm $tmpfile

echo
printf "\e[32m$pass passed\e[m, \e[31m$fail fails\e[m\n"
