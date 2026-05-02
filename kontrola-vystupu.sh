#!/bin/bash
# IOS 2026 - kontrolni skript pro syntaktickou spravnost vystupu
# Pouziti: ./kontrola-vystupu.sh < proj2.out
# Pripadne chyby piste do fora predmetu na moodle.vut.cz

tmp1=`mktemp`
tmp2=`mktemp`

cat > $tmp1
echo "--------------------------------------"
echo "Chybi nasledujici typy radek:"

#---------------
Q="^[1-9][0-9]*: D: started$"
grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: D: started"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: D: next cart$"
grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: D: next cart"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: D: closing$"
grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: D: closing"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: V [1-9][0-9]*: started$"

grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: V idV: started"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: V [1-9][0-9]*: boarding started$"

grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: V idV: boarding started"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: V [1-9][0-9]*: boarding complete$"

grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: V idV: boarding complete"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: V [1-9][0-9]*: leaving started$"

grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: V idV: leaving started"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: V [1-9][0-9]*: leaving complete$"

grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: V idV: leaving complete"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: V [1-9][0-9]*: closed$"

grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: V idV: closed"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: N [1-9][0-9]*: started$"
grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: N idN: started"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: N [1-9][0-9]*: queue$"
grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: N idN: queue"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: N [1-9][0-9]*: boarding$"
grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: N idN: boarding"
fi

cat < $tmp2 >$tmp1
#---------------
Q="^[1-9][0-9]*: N [1-9][0-9]*: leaving$"
grep -v "$Q" <$tmp1 >$tmp2

if grep "$Q" < $tmp1 > /dev/null; 
then true
else echo "A: N idN: leaving"
fi

cat < $tmp2 >$tmp1


echo "--------------------------------------"
echo "Nasledujci radky jsou syntakticky chybne, nebo ve vystupu prebyvaji:"

cat < $tmp1

