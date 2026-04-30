#!/bin/bash

if [ "$#" -lt 3 ]; then
    exit 1
fi

N=$1
T=$2
MODULO=$3
RUNS=${4:-10}

EXEC="./dynamic" 

if [ ! -f "$EXEC" ]; then
    exit 1
fi

CSV_FILE="benchmark_N${N}_t${T}_mod${MODULO}.csv"

echo "Run,Setup(ms),KeyGen(ms),ShareGenAvg(ms),ShareGenTotal(ms),Reconstruct(ms),TotalMB" > "$CSV_FILE"

for (( i=1; i<=$RUNS; i++ ))
do
    RESULT=$($EXEC $N $T $MODULO | tail -n 1)
    echo "$i,$RESULT" >> "$CSV_FILE"
done

awk -F',' '
NR>1 {
    for(i=2; i<=NF; i++) sum[i]+=$i
}
END {
    printf "AVG"
    for(i=2; i<=NF; i++) printf ",%.4f", sum[i]/(NR-1)
    print ""
}' "$CSV_FILE" >> "$CSV_FILE"