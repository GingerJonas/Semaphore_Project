#!/bin/bash
# =============================================================================
# Testovaci skript pro proj2 (Horna draha / Rollercoaster)
# Pouziti: ./testy.sh [cesta/k/proj2] [cesta/k/kontrola-vystupu.sh]
# Defaultne predpoklada ./proj2 a ./kontrola-vystupu.sh ve stejnem adresari
# =============================================================================

PROJ2="${1:-./proj2}"
KONTROLA="${2:-./kontrola-vystupu.sh}"
OUTFILE="proj2.out"

PASS=0
FAIL=0
TOTAL=0

# Barvy pro terminal
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# =============================================================================
# Pomocne funkce
# =============================================================================

pass() {
    echo -e "  ${GREEN}[PASS]${NC} $1"
    PASS=$((PASS + 1))
    TOTAL=$((TOTAL + 1))
}

fail() {
    echo -e "  ${RED}[FAIL]${NC} $1"
    FAIL=$((FAIL + 1))
    TOTAL=$((TOTAL + 1))
}

section() {
    echo ""
    echo -e "${YELLOW}=== $1 ===${NC}"
}

# Spusti proj2, pocka na dokonceni (max N sekund), vrati exit kod
run_with_timeout() {
    local timeout_sec="$1"
    shift
    local spinner='|/-\'
    local i=0

    timeout "$timeout_sec" "$PROJ2" "$@" &
    local job_pid=$!

    while kill -0 "$job_pid" 2>/dev/null; do
        i=$(( (i+1) % 4 ))
        printf "\r    Bezi... %s (max %ds)" "${spinner:$i:1}" "$timeout_sec" >&2
        sleep 0.2
    done
    printf "\r                              \r" >&2

    wait "$job_pid"
    return $?
}

# Zkontroluje syntaxi vystupu pres kontrola-vystupu.sh
# Vrati 0 pokud jsou chybejici radky prazdne a prebyvajici radky prazdne
check_syntax() {
    local outfile="$1"
    local result
    result=$(bash "$KONTROLA" < "$outfile" 2>/dev/null)

    # Extrakt sekce "Chybi" -- preskoci separator radky (---) a hlavicku
    # Format vystupu: "------" / "Chybi nasledujici typy radek:" / [chybejici] / "------"
    local missing
    missing=$(echo "$result" | awk '
        /Chybi nasledujici/ { found=1; next }
        /^-+$/ { found=0; next }
        /Nasledujci radky/ { found=0; next }
        found && NF
    ')

    # Extrakt sekce "Prebyvaji" -- vse po posledni hlavicce
    local extra
    extra=$(echo "$result" | awk '
        /Nasledujci radky jsou syntakticky/ { found=1; next }
        found && NF
    ')

    if [ -z "$missing" ] && [ -z "$extra" ]; then
        return 0
    else
        echo "    Syntakticke chyby:"
        [ -n "$missing" ] && echo "    Chybi: $missing"
        [ -n "$extra"   ] && echo "    Prebyvaji: $extra"
        return 1
    fi
}

# Zkontroluje ze action counter je sekvencni bez mezer a zacina od 1
check_sequential_counter() {
    local outfile="$1"
    local prev=0
    local ok=true
    while IFS= read -r line; do
        local num
        num=$(echo "$line" | grep -oP '^\d+')
        [ -z "$num" ] && continue
        if [ "$num" -ne $((prev + 1)) ]; then
            echo "    Ocekavan action number $((prev+1)), nalezen $num"
            ok=false
            break
        fi
        prev=$num
    done < "$outfile"
    $ok && return 0 || return 1
}

# Zkontroluje ze vsichni N navstevnici maji vsechny 4 vypisy
check_all_visitors() {
    local outfile="$1"
    local N="$2"
    local ok=true
    for i in $(seq 1 "$N"); do
        for event in "started" "queue" "boarding" "leaving"; do
            if ! grep -qP "^\d+: N $i: $event$" "$outfile"; then
                echo "    Chybi: N $i: $event"
                ok=false
            fi
        done
    done
    $ok && return 0 || return 1
}

# Zkontroluje ze vsechny V voziky maji vsechny vypisy
check_all_carts() {
    local outfile="$1"
    local V="$2"
    local ok=true
    for i in $(seq 1 "$V"); do
        for event in "started" "closed"; do
            if ! grep -qP "^\d+: V $i: $event$" "$outfile"; then
                echo "    Chybi: V $i: $event"
                ok=false
            fi
        done
    done
    $ok && return 0 || return 1
}

# Zkontroluje ze zadne "N X: boarding" neni mimo "boarding started" a "boarding complete"
# a zadne "N X: leaving" neni mimo "leaving started" a "leaving complete"
check_boarding_leaving_order() {
    local outfile="$1"
    local boarding_open=false
    local leaving_open=false
    local ok=true

    while IFS= read -r line; do
        if echo "$line" | grep -qP ': V \d+: boarding started$'; then
            boarding_open=true
        fi
        if echo "$line" | grep -qP ': V \d+: boarding complete$'; then
            boarding_open=false
        fi
        if echo "$line" | grep -qP ': V \d+: leaving started$'; then
            leaving_open=true
        fi
        if echo "$line" | grep -qP ': V \d+: leaving complete$'; then
            leaving_open=false
        fi
        if echo "$line" | grep -qP ': N \d+: boarding$'; then
            if [ "$boarding_open" = false ]; then
                echo "    Naruseni poradi: $line"
                ok=false
            fi
        fi
        if echo "$line" | grep -qP ': N \d+: leaving$'; then
            if [ "$leaving_open" = false ]; then
                echo "    Naruseni poradi: $line"
                ok=false
            fi
        fi
    done < "$outfile"

    $ok && return 0 || return 1
}

# Spusti kompletni funkcni test pro dany set parametru, N-krat
functional_test() {
    local label="$1"
    local V="$2"
    local N="$3"
    local K="$4"
    local TV="$5"
    local TN="$6"
    local O="$7"
    local runs="${8:-3}"
    local timeout_sec="${9:-15}"

    echo ""
    echo "  Parametry: V=$V N=$N K=$K TV=$TV TN=$TN O=$O (${runs}x)"

    local all_ok=true

    for run in $(seq 1 "$runs"); do
        run_with_timeout "$timeout_sec" "$V" "$N" "$K" "$TV" "$TN" "$O" > /dev/null 2>&1
        local exit_code=$?

        if [ "$exit_code" -eq 124 ]; then
            echo "    Beh $run: TIMEOUT po ${timeout_sec}s"
            all_ok=false
            continue
        fi

        if [ ! -f "$OUTFILE" ]; then
            echo "    Beh $run: $OUTFILE nebyl vytvoren"
            all_ok=false
            continue
        fi

        # Syntakticka kontrola
        if ! check_syntax "$OUTFILE"; then
            echo "    Beh $run: syntakticka chyba"
            all_ok=false
            continue
        fi

        # Sekvencni citac
        if ! check_sequential_counter "$OUTFILE"; then
            echo "    Beh $run: nesekvencni action counter"
            all_ok=false
            continue
        fi

        # Vsichni navstevnici
        local issues
        issues=$(check_all_visitors "$OUTFILE" "$N" 2>&1)
        if [ $? -ne 0 ]; then
            echo "    Beh $run: chybejici vypisy navstevniku"
            echo "$issues"
            all_ok=false
            continue
        fi

        # Vsechny voziky
        issues=$(check_all_carts "$OUTFILE" "$V" 2>&1)
        if [ $? -ne 0 ]; then
            echo "    Beh $run: chybejici vypisy voziku"
            echo "$issues"
            all_ok=false
            continue
        fi

        # Poradi boarding/leaving
        issues=$(check_boarding_leaving_order "$OUTFILE" 2>&1)
        if [ $? -ne 0 ]; then
            echo "    Beh $run: naruseni poradi boarding/leaving"
            echo "$issues"
            all_ok=false
            continue
        fi

        # D: started, D: next cart, D: closing
        if ! grep -qP '^\d+: D: started$' "$OUTFILE"; then
            echo "    Beh $run: chybi D: started"
            all_ok=false; continue
        fi
        if ! grep -qP '^\d+: D: closing$' "$OUTFILE"; then
            echo "    Beh $run: chybi D: closing"
            all_ok=false; continue
        fi

    done

    if $all_ok; then
        pass "$label"
    else
        fail "$label"
    fi
}

# =============================================================================
# ZKONTROLUJ PREREKVIZITY
# =============================================================================
section "Prerekvizity"

if [ ! -f "$PROJ2" ]; then
    echo -e "${RED}CHYBA: Nenalezen binarni soubor '$PROJ2'${NC}"
    echo "Prelozit pomoci 'make' nejdrive."
    exit 1
fi
echo "  Nalezen: $PROJ2"

if [ ! -f "$KONTROLA" ]; then
    echo -e "${RED}CHYBA: Nenalezen '$KONTROLA'${NC}"
    exit 1
fi
echo "  Nalezen: $KONTROLA"

# =============================================================================
# TESTY ARGUMENTU -- ocekavany exit code 1
# =============================================================================
section "Testy neplatnych argumentu (ocekavan exit code 1)"

arg_test() {
    local label="$1"
    shift
    "$PROJ2" "$@" > /dev/null 2>&1
    local code=$?
    if [ "$code" -eq 1 ]; then
        pass "$label"
    else
        fail "$label (exit code byl $code, ocekavan 1)"
    fi
}

arg_test "Malo argumentu (5)"          2 10 4 10 10
arg_test "Moc argumentu (7)"           2 10 4 10 10 10 99
arg_test "V = 0 (pod rozsahem)"        0 10 4 10 10 10
arg_test "V = 10 (nad rozsahem)"       10 10 4 10 10 10
arg_test "V = -1 (zaporne)"           -1 10 4 10 10 10
arg_test "V = neni cislo"              a 10 4 10 10 10
arg_test "N = 0 (pod rozsahem)"        2 0 4 10 10 10
arg_test "N = 10000 (nad rozsahem)"    2 10000 4 10 10 10
arg_test "N = -1 (zaporne)"            2 -1 4 10 10 10
arg_test "N = neni cislo"              2 x 4 10 10 10
arg_test "K = 3 (pod rozsahem)"        2 10 3 10 10 10
arg_test "K = 41 (nad rozsahem)"       2 10 41 10 10 10
arg_test "K = -4 (zaporne)"            2 10 -4 10 10 10
arg_test "K = neni cislo"              2 10 z 10 10 10
arg_test "TV = 1001 (nad rozsahem)"    2 10 4 1001 10 10
arg_test "TV = neni cislo"             2 10 4 abc 10 10
arg_test "TN = 1001 (nad rozsahem)"    2 10 4 10 1001 10
arg_test "TN = neni cislo"             2 10 4 10 xyz 10
arg_test "O = 0 (pod rozsahem)"        2 10 4 10 10 0
arg_test "O = 101 (nad rozsahem)"      2 10 4 10 10 101
arg_test "O = -5 (zaporne)"            2 10 4 10 10 -5
arg_test "O = neni cislo"              2 10 4 10 10 q

# =============================================================================
# TESTY PLATNYCH ARGUMENTU -- ocekavan exit code 0
# =============================================================================
section "Testy hranicnich platnych argumentu (ocekavan exit code 0)"

valid_arg_test() {
    local label="$1"
    shift
    timeout 15 "$PROJ2" "$@" > /dev/null 2>&1
    local code=$?
    if [ "$code" -eq 0 ]; then
        pass "$label"
    else
        fail "$label (exit code byl $code, ocekavan 0)"
    fi
}

valid_arg_test "Minimalni V=1"          1 5 4 10 10 10
valid_arg_test "Maximalni V=9"          9 5 4 10 10 10
valid_arg_test "Minimalni N=1"          2 1 4 10 10 10
valid_arg_test "Minimalni K=4"          2 5 4 10 10 10
valid_arg_test "Maximalni K=40"         2 5 40 10 10 10
valid_arg_test "TV=0 (povoleno)"        2 5 4 0 10 10
valid_arg_test "TV=1000 (maximum)"      2 5 4 1000 10 10
valid_arg_test "TN=0 (povoleno)"        2 5 4 10 0 10
valid_arg_test "TN=1000 (maximum)"      2 5 4 10 1000 10
valid_arg_test "O=1 (minimum)"          2 5 4 10 10 1
valid_arg_test "O=100 (maximum)"        2 5 4 10 10 100

# =============================================================================
# FUNKCNI TESTY
# =============================================================================
section "Funkcni testy -- referencni priklad ze zadani"
functional_test "Referencni: 2 10 4 10 10 10" 2 10 4 10 10 10 5

section "Funkcni testy -- castecna jizda (N < K)"
functional_test "N=1, K=4: jeden navstevnik sam" 1 1 4 10 10 10 3
functional_test "N=3, K=4: mene nez kapacita"   2 3 4 10 10 10 3
functional_test "N=7, K=4: jedna plna + jedna castecna jizda" 2 7 4 10 10 10 3

section "Funkcni testy -- presne N = K"
functional_test "N=K=4: jedna presne plna jizda" 1 4 4 10 10 10 3
functional_test "N=8, K=4: dve presne plne jizdy" 2 8 4 10 10 10 3

section "Funkcni testy -- vice voziku"
functional_test "V=5, N=20, K=4" 5 20 4 10 10 10 3
functional_test "V=9, N=9, K=4: kazdy vozik jede max jednou" 9 9 4 10 10 10 3

section "Funkcni testy -- extremni casovani"
functional_test "TV=0, TN=0, O=1: vsechno okamzite" 2 10 4 0 0 1 5
functional_test "TV=1000, TN=1000, O=100: vsechno pomale" 2 8 4 1000 1000 100 3 30
functional_test "TN=0: navstevnici prichazi okamzite" 3 12 4 10 0 10 3

section "Funkcni testy -- velky stres"
functional_test "V=9, N=100, K=40" 9 100 40 10 10 10 3 30
functional_test "V=9, N=9999, K=40" 9 9999 40 0 0 1 2 60

section "Funkcni testy -- jeden vozik"
functional_test "V=1, N=10, K=4" 1 10 4 10 10 10 3
functional_test "V=1, N=1, K=40: jeden navstevnik, velka kapacita" 1 1 40 10 10 10 3

section "Opakovane behy (kontrola nedeterminismu)"
echo "  Stejne parametry, 10 behu -- kontrola konzistence"
NDETERM_FAIL=0
for i in $(seq 1 10); do
    timeout 10 "$PROJ2" 3 15 4 10 10 10 > /dev/null 2>&1
    code=$?
    if [ "$code" -eq 124 ]; then
        echo "    Beh $i: TIMEOUT"
        NDETERM_FAIL=$((NDETERM_FAIL + 1))
    elif [ ! -f "$OUTFILE" ]; then
        echo "    Beh $i: chybi vystupni soubor"
        NDETERM_FAIL=$((NDETERM_FAIL + 1))
    else
        if ! check_syntax "$OUTFILE" > /dev/null 2>&1 || \
           ! check_sequential_counter "$OUTFILE" > /dev/null 2>&1 || \
           ! check_boarding_leaving_order "$OUTFILE" > /dev/null 2>&1; then
            echo "    Beh $i: chyba v poradi nebo syntaxi"
            NDETERM_FAIL=$((NDETERM_FAIL + 1))
        fi
    fi
done
if [ "$NDETERM_FAIL" -eq 0 ]; then
    pass "10 opakovanych behu -- vsechny ok"
else
    fail "10 opakovanych behu -- $NDETERM_FAIL selhalo"
fi

# =============================================================================
# SOUHRN
# =============================================================================
section "Souhrn"
echo ""
echo -e "  Celkem:  $TOTAL"
echo -e "  ${GREEN}Uspesne: $PASS${NC}"
if [ "$FAIL" -gt 0 ]; then
    echo -e "  ${RED}Neuspesne: $FAIL${NC}"
else
    echo -e "  Neuspesne: $FAIL"
fi
echo ""
if [ "$FAIL" -eq 0 ]; then
    echo -e "${GREEN}Vsechny testy prosly!${NC}"
else
    echo -e "${RED}Nektera testy selhala.${NC}"
fi
