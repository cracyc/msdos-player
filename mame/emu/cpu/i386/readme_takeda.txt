Based on MAME 0.152.
Fixes in MAME 0.154 to 0.279 are applied.

Mr. cracyc's fix is applied:
https://github.com/mamedev/mame/commit/eeb81cdf30f80cd7ce797c4574b8534dd4cddc46

i386_state are removed and all its members are changed to global variables.
All registers can be accessed directly without cpustate->.

cycle_table_rm/pm are changed from dynamic array to static array.

i386_limit_check is improved to consider data size.

Modified to raise INT 06h (illegal opcode) after all params are fetched
when ignore_illegal_insn is true.
This is to get the EIP of the next opcode as correct as possible.

Mr.Jason Hood fixed to use unaligned reads/writes only when necessary
for a minor speed improvement.
