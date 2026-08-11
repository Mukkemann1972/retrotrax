; rtx_amiga Phase 1 - minimalster Beweis: Paula-Kanal 0 direkt ansteuern.
; Reiner Bootblock (kein AmigaOS-Executable, keine Exec/DOS-Aufrufe) -
; spielt ein eingebettetes 8-bit-Wellenform-Sample einmal, dann in Dauerschleife
; (Paula wiederholt den DMA-Puffer automatisch, solange DMA aktiv bleibt).
;
; Einsprung laut AmigaOS-Bootprozess: direkt bei "start", A1 = geoeffnetes
; trackdisk.device-IORequest (hier ungenutzt). Wir kehren nie zurueck (kein rts),
; deshalb ist die Rueckgabe-Konvention (D0/A0) irrelevant.

CUSTOM  equ $dff000
DMACON  equ CUSTOM+$096
AUD0LC  equ CUSTOM+$0a0        ; volles Long: AUD0LCH+AUD0LCL zusammen
AUD0LEN equ CUSTOM+$0a4
AUD0PER equ CUSTOM+$0a6
AUD0VOL equ CUSTOM+$0a8

        section bootblock,code

start:
        lea     wave(pc),a0
        move.l  a0,AUD0LC              ; Sample-Adresse (PC-relativ -> egal wohin geladen)
        move.w  #wavewords,AUD0LEN     ; Laenge in WORDS (2 Bytes = 2 Samples/Wort)
        move.w  #period,AUD0PER        ; Abspielperiode -> Tonhoehe
        move.w  #64,AUD0VOL            ; volle Lautstaerke (0-64)
        move.w  #%1000001000000001,DMACON ; SET-Flag(15) + DMAEN(9) + AUD0EN(0)

hang:
        bra.s   hang

period  equ 400                        ; grobe Testtonhoehe, nicht musikalisch kalibriert

wave:
        ; ein Zyklus Rechteckwelle, 8-bit signed PCM, Amiga-Paula-natives Format
        dc.b 100,100,100,100,100,100,100,100
        dc.b 100,100,100,100,100,100,100,100
        dc.b -100,-100,-100,-100,-100,-100,-100,-100
        dc.b -100,-100,-100,-100,-100,-100,-100,-100
waveend:

wavewords equ (waveend-wave)/2
