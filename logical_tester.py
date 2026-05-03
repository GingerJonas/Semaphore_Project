import os
import subprocess
import re
import tempfile
import shutil

# --- KONFIGURACE ---
BINARY_PATH = "./proj2"

# Test cases: (V, N, K, TV, TN, O, je_validni, popis)
TEST_CASES = [
    # --- ZÁKLADNÍ TESTY ---
    (1, 5, 5, 10, 10, 10, True, "Základní test (přesně 1 vozík plný)"),
    (2, 5, 10, 10, 10, 10, True, "Méně lidí než kapacita (částečně zaplněný vozík)"),
    (3, 20, 5, 50, 50, 10, True, "Více vozíků, plné vytížení"),
    (2, 10, 4, 10, 10, 10, True, "Test ze zadání"),

    # --- HRANIČNÍ A EDGE CASES ---
    (1, 1, 4, 0, 0, 1, True, "Absolutní minimum (1 vozík, 1 člověk, min kapacita)"),
    (9, 100, 40, 0, 0, 1, True, "Maximum vozíků a max kapacita (rychlý průběh)"),
    (3, 40, 40, 0, 0, 1, True, "Kapacita se rovná počtu lidí (1 jízda, ostatní zjistí prázdno)"),
    (3, 17, 5, 10, 10, 1, True, "Prvočísla: Není dělitelné K ani V (Vozík 1 pojede dvakrát, podruhé částečně plný)"),
    (4, 20, 5, 5, 5, 1, True, "Dokonalé rozdělení: 4 vozíky, každý přesně 1 plná jízda"),
    (2, 15, 5, 0, 100, 1, True, "Pomalí návštěvníci, bleskové vozíky (musí čekat na lidi)"),
    (2, 15, 5, 100, 0, 1, True, "Bleskoví návštěvníci, pomalé vozíky (hromadí se ve frontě)"),
    (5, 4, 5, 10, 10, 1, True, "Zavírání parku: 5 vozíků, jen 4 lidi. (Ukončení prázdných vozíků)"),
    (1, 9999, 40, 0, 0, 1, True, "Thundering Herd: 9999 lidí bez uspávání se rve o 40 míst."),
    (3, 12, 4, 1000, 0, 1, True, "Sobecký vozík: Zákaz předjíždění v cílové stanici i přes zpoždění."),

    # --- CHYBOVÉ STAVY (ZDE JE O=0 SPRÁVNĚ, PROTOŽE SE OČEKÁVÁ ODMÍTNUTÍ PROGRAMEM) ---
    (0, 5, 5, 0, 0, 10, False, "Nevalidní argumenty: V=0 -> Očekáván Error"),
    (10, 5, 5, 0, 0, 10, False, "Nevalidní argumenty: V=10 -> Očekáván Error"),
    (2, 0, 5, 0, 0, 10, False, "Nevalidní argumenty: N=0 -> Očekáván Error"),
    (2, 5, 3, 0, 0, 10, False, "Nevalidní argumenty: K=3 (pod limit) -> Očekáván Error"),
    (2, 5, 41, 0, 0, 10, False, "Nevalidní argumenty: K=41 (nad limit) -> Očekáván Error"),
    (2, 5, 5, -1, 0, 10, False, "Nevalidní argumenty: TV=-1 -> Očekáván Error"),
    (2, 5, 5, 0, 1001, 10, False, "Nevalidní argumenty: TN=1001 -> Očekáván Error"),
    (2, 5, 5, 0, 0, 0, False, "Nevalidní argumenty: O=0 -> Očekáván Error"),
    (2, 5, 5, 0, 0, 101, False, "Nevalidní argumenty: O=101 -> Očekáván Error"),
]

def check_leftovers():
    print("\n📊 Kontrola zombíků a nezavřené paměti:")
    subprocess.run("pgrep -x proj2 | wc -l | awk '{print \"   Zbyva bezicich/zombie procesu proj2: \" $1}'", shell=True)
    subprocess.run("ipcs -m 2>/dev/null | grep \"$(whoami)\" | wc -l | awk '{print \"   Zbyva alokovanych sdilenych pameti: \" $1}'", shell=True)
    subprocess.run("ipcs -s 2>/dev/null | grep \"$(whoami)\" | wc -l | awk '{print \"   Zbyva alokovanych semaforu: \" $1}'", shell=True)
    subprocess.run("find /dev/shm -user \"$(whoami)\" 2>/dev/null | wc -l | awk '{print \"   Zbyva POSIX semaforu/pameti: \" $1}'", shell=True)

def cleanup():
    subprocess.run("killall -q -9 proj2", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run("ipcs -tm 2>/dev/null | grep \"$(whoami)\" | awk '{print $1}' | xargs -r ipcrm -m 2>/dev/null", shell=True)
    subprocess.run("ipcs -ts 2>/dev/null | grep \"$(whoami)\" | awk '{print $1}' | xargs -r ipcrm -s 2>/dev/null", shell=True)
    subprocess.run("find /dev/shm -user \"$(whoami)\" -delete 2>/dev/null", shell=True)

def parse_and_validate(out_file, V, N, K):
    with open(out_file, "r") as f:
        lines = f.readlines()

    line_counter = 1
    
    # --- STAVOVÉ PROMĚNNÉ ---
    visitors = {i: "NOT_STARTED" for i in range(1, N + 1)}
    carts = {i: "NOT_STARTED" for i in range(1, V + 1)}
    cart_passengers = {i: set() for i in range(1, V + 1)} # Kdo fyzicky sedí ve kterém vozíku
    dispatcher_state = "NOT_STARTED"
    
    active_boarding_cart = None
    active_leaving_cart = None
    departed_carts = [] # Fronta vozíků, co jsou zrovna na trati
    park_closed = False
    
    # Hlídač povelů dispečera
    unclaimed_dispatcher_signals = 0 
    
    for line_num, line in enumerate(lines, 1):
        line = line.strip()
        if not line: continue
        
        # --- 1. KONTROLA PŘESNÉHO FORMÁTU ---
        # Regulární výraz zachytí "A: D: akce" i "A: V id: akce" i "A: N id: akce"
        match = re.match(r"^(\d+):\s+([DNV])\s*(\d*):\s+(.+)$", line)
        if not match:
            return f"Řádek {line_num}: Špatný formát textu zadání -> '{line}'"
        
        A = int(match.group(1))
        entity = match.group(2)
        eid = int(match.group(3)) if match.group(3) else None
        action = match.group(4)
        
        # --- 2. KONTROLA ČÍSLOVÁNÍ (A) ---
        if A != line_counter:
            return f"Řádek {line_num}: Špatné číslování akce! Očekáváno {line_counter}, nalezeno {A}."
        line_counter += 1

        # --- 3. LOGIKA DISPEČERA ---
        if entity == "D":
            if action == "started":
                if dispatcher_state != "NOT_STARTED": return f"Řádek {line_num}: Dispečer zapnut podruhé!"
                dispatcher_state = "started"
                
            elif action == "next cart":
                if park_closed: return f"Řádek {line_num}: Dispečer dává 'next cart', i když už je park zavřený!"
                unclaimed_dispatcher_signals += 1
                
            elif action == "closing":
                park_closed = True
                dispatcher_state = "closing"
                
            else:
                return f"Řádek {line_num}: Neznámý text akce dispečera -> '{action}'"

        # --- 4. LOGIKA VOZÍKU ---
        elif entity == "V":
            if eid not in carts: return f"Řádek {line_num}: Neznámé ID vozíku V {eid}"
            
            if action == "started":
                if carts[eid] != "NOT_STARTED": return f"Řádek {line_num}: Vozík {eid} zapnut podruhé!"
                carts[eid] = "waiting"
                
            elif action == "boarding started":
                if carts[eid] not in ["waiting", "finished"]: return f"Řádek {line_num}: Vozík {eid} nemůže začít boarding ze stavu {carts[eid]}."
                if active_boarding_cart is not None: return f"Řádek {line_num}: Vozík {eid} začal boarding, ale stanici ještě blokuje vozík {active_boarding_cart}."
                if park_closed: return f"Řádek {line_num}: FATÁLNÍ! Vozík {eid} otevřel dveře, ale park už je zavřený ('closing')!"
                
                if unclaimed_dispatcher_signals <= 0:
                    return f"Řádek {line_num}: FATÁLNÍ! Vozík {eid} otevřel dveře ('boarding started'), aniž by mu k tomu dal dispečer pokyn ('next cart')!"
                
                unclaimed_dispatcher_signals -= 1 # Vozík si "vyzvedl" povel
                active_boarding_cart = eid
                carts[eid] = "boarding"
                cart_passengers[eid].clear()
                
            elif action == "boarding complete":
                if active_boarding_cart != eid: return f"Řádek {line_num}: Vozík {eid} hlásí boarding complete, ale stanice eviduje aktivní vozík {active_boarding_cart}."
                carts[eid] = "on_track"
                active_boarding_cart = None
                departed_carts.append(eid)
                
            elif action == "leaving started":
                if carts[eid] != "on_track": return f"Řádek {line_num}: Vozík {eid} začal leaving, ale není vůbec na trati!"
                if departed_carts[0] != eid: return f"Řádek {line_num}: Předjíždění! Očekával se dojezd vozíku {departed_carts[0]}, ale přijel dřív vozík {eid}."
                if active_leaving_cart is not None: return f"Řádek {line_num}: Vozík {eid} začal leaving, ale výstupní stanici blokuje vozík {active_leaving_cart}."
                
                active_leaving_cart = eid
                carts[eid] = "leaving"
                
            elif action == "leaving complete":
                if active_leaving_cart != eid: return f"Řádek {line_num}: Vozík {eid} ukončil leaving, ale vykládal se vozík {active_leaving_cart}."
                if len(cart_passengers[eid]) > 0: return f"Řádek {line_num}: FATÁLNÍ! Vozík {eid} odjel z výstupní stanice, ale nechal uvnitř nevystoupené lidi!"
                
                carts[eid] = "waiting"
                active_leaving_cart = None
                departed_carts.pop(0)
                
            elif action == "closed":
                if carts[eid] != "waiting": return f"Řádek {line_num}: Vozík {eid} se ukončuje ('closed'), ale není ve stavu 'waiting' (aktuálně je: {carts[eid]})."
                carts[eid] = "closed"
                
            else:
                return f"Řádek {line_num}: Neznámý text akce vozíku -> '{action}'"

        # --- 5. LOGIKA NÁVŠTĚVNÍKA ---
        elif entity == "N":
            if eid not in visitors: return f"Řádek {line_num}: Neznámé ID návštěvníka N {eid}"
            
            if action == "started":
                if visitors[eid] != "NOT_STARTED": return f"Řádek {line_num}: Návštěvník {eid} zapnut podruhé!"
                visitors[eid] = "started"
                
            elif action == "queue":
                if visitors[eid] != "started": return f"Řádek {line_num}: Návštěvník {eid} šel do fronty z chybného stavu {visitors[eid]}."
                visitors[eid] = "queue"
                
            elif action == "boarding":
                if visitors[eid] != "queue": return f"Řádek {line_num}: Návštěvník {eid} nastupuje bez čekání ve frontě!"
                if active_boarding_cart is None: return f"Řádek {line_num}: Návštěvník {eid} nastupuje, ale dveře stanice jsou zavřené (žádný vozík nedal 'boarding started')!"
                
                visitors[eid] = f"in_cart_{active_boarding_cart}"
                cart_passengers[active_boarding_cart].add(eid)
                if len(cart_passengers[active_boarding_cart]) > K:
                    return f"Řádek {line_num}: Překročena kapacita! Vozík {active_boarding_cart} má v sobě víc než {K} lidí (limit K={K})."
                    
            elif action == "leaving":
                expected_cart = active_leaving_cart
                if expected_cart is None: return f"Řádek {line_num}: Návštěvník {eid} vystupuje, ale na stanici žádný vozík nedal 'leaving started'!"
                if eid not in cart_passengers[expected_cart]: return f"Řádek {line_num}: Návštěvník {eid} vystupuje z vozíku {expected_cart}, ale fyzicky do něj nikdy nenastoupil!"
                
                visitors[eid] = "leaving"
                cart_passengers[expected_cart].remove(eid) # Návštěvník fyzicky opustil paměť vozíku
                
            else:
                return f"Řádek {line_num}: Neznámý text akce návštěvníka -> '{action}'"

    # --- 6. ZÁVĚREČNÁ KONTROLA PO UKONČENÍ PROGRAMU ---
    for eid, state in visitors.items():
        if state != "leaving":
            return f"Konec souboru: Návštěvník N {eid} neukončil proces úspěšně! Zůstal viset ve stavu: '{state}'"
            
    for eid, state in carts.items():
        if state != "closed":
            return f"Konec souboru: Vozík V {eid} neukončil proces úspěšně! Zůstal viset ve stavu: '{state}'"
    
    if active_boarding_cart is not None:
        return f"Konec souboru: Uvízlý stav! Vozík {active_boarding_cart} nikdy nedokončil boarding."
    if active_leaving_cart is not None:
        return f"Konec souboru: Uvízlý stav! Vozík {active_leaving_cart} nikdy nedokončil leaving."
        
    return "OK"

def run_logic_tests():
    subprocess.run("make clean", shell=True)
    subprocess.run("make", shell=True)
    if not os.path.exists(BINARY_PATH):
        print("❌ Binárka neexistuje.")
        return
    
    print("🧹 Provádím úklid před testy...")
    cleanup()

    print("🧩 Spouštím logické ověření stavového automatu...")
    
    passed = 0
    for V, N, K, TV, TN, O, expected_valid, desc in TEST_CASES:
        args = [str(x) for x in (V, N, K, TV, TN, O)]
        print(f"\nTest: {desc}")
        
        with tempfile.TemporaryDirectory() as temp_dir:
            local_binary = os.path.join(temp_dir, "proj2")
            shutil.copy(BINARY_PATH, local_binary)
            
            try:
                proc = subprocess.run(
                    [local_binary] + args,
                    cwd=temp_dir,
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                
                out_file = os.path.join(temp_dir, "proj2.out")
                
                # --- Kontrola chybových scénářů (nevalidní argumenty) ---
                if not expected_valid:
                    if proc.returncode != 0:
                        print(f"✅ OK: Program správně odmítl nevalidní vstup (Exit code: {proc.returncode}).")
                        passed += 1
                    else:
                        print("❌ SELHÁNÍ LOGIKY: Program vrátil exit 0, přestože měl vstup zamítnout!")
                    continue
                
                # --- Kontrola validních scénářů ---
                if proc.returncode != 0:
                    print(f"❌ SELHÁNÍ: Program nečekaně havaroval nebo vrátil chybu (Exit code {proc.returncode}). Stderr: {proc.stderr.strip()}")
                    continue

                if not os.path.exists(out_file):
                    print("❌ SELHÁNÍ: Program vrátil 0, ale nevytvořil soubor proj2.out!")
                    continue
                
                # Analýza výstupního souboru logickým parserem
                result = parse_and_validate(out_file, V, N, K)
                if result == "OK":
                    print("✅ LOGIKA V POŘÁDKU")
                    passed += 1
                else:
                    print(f"❌ SELHÁNÍ LOGIKY:\n   -> {result}")
                    
            except subprocess.TimeoutExpired:
                 print("❌ DEADLOCK (Timeout - uvízlo na semaforech)")
                 cleanup()
                 
    print("\n" + "="*50)
    print(f"🎯 SUMA: {passed} / {len(TEST_CASES)} logických testů prošlo.")
    print("="*50)

    check_leftovers()
    
    print("\n🧹 Finální úklid a ukončení skriptu...")
    cleanup()

if __name__ == "__main__":
    try:
        run_logic_tests()
    except KeyboardInterrupt:
        print("\n⚠️ Přerušeno uživatelem! Uklízím bordel...")
        cleanup()
