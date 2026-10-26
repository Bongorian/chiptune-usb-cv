#!/usr/bin/env python3
"""Execute actual ARM main-loop clock/bus control. MMIO aliases are modeled;
PIO edges, IC behavior, ROM latency and device deadlines are not emulated."""
import argparse,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UC_HOOK_MEM_INVALID
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
from check_arm_boot import Image,BANK
from arm_support import runtime_hooks

def verify(elf):
    from elftools.elf.elffile import ELFFile
    fields={}
    with elf.open('rb') as stream:
        dwarf=ELFFile(stream).get_dwarf_info()
        for cu in dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                name=die.attributes.get('DW_AT_name')
                if die.tag!='DW_TAG_typedef' or not name:continue
                name=name.value.decode()
                if name not in ['Engine','Project','SnClockConfig','SnModConfig','RegisterBus','SnClockCounter']:continue
                structure=die.get_DIE_from_attribute('DW_AT_type')
                for member in structure.iter_children():
                    if member.tag=='DW_TAG_member':fields[name+'.'+member.attributes['DW_AT_name'].value.decode()]=member.attributes['DW_AT_data_member_location'].value
    image=Image(elf);image.boot(b'\xff'*(2*BANK))
    source=image.last_machine; uc=image.machine();uc.mem_map(0x50200000,0x4000);uc.mem_write(0x20000000,bytes(source.mem_read(0x20000000,0x42000)))
    def invalid(m,access,address,size,value,data):
        raise AssertionError(f"Unmapped {address:#x}, PC={m.reg_read(UC_ARM_REG_PC):#x}, LR={m.reg_read(UC_ARM_REG_LR):#x}")
    uc.hook_add(UC_HOOK_MEM_INVALID,invalid)
    runtime_hooks(image,uc)
    now=100000;returned=False
    def ret(m,v=0):
        m.reg_write(UC_ARM_REG_R0,v&0xffffffff);m.reg_write(UC_ARM_REG_R1,v>>32);m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    def timer(m,*_):ret(m,now)
    image.hook(uc,'time_us_64',timer)
    def delay(m,*_):
        nonlocal now
        now+=m.reg_read(UC_ARM_REG_R0);uc.mem_write(0x40054028,struct.pack('<I',now&0xffffffff));ret(m)
    image.hook(uc,'busy_wait_us_32',delay)
    def stop(m,*_):
        nonlocal returned
        returned=True;m.emu_stop()
    uc.hook_add(UC_HOOK_CODE,stop,begin=0x200,end=0x200)
    def aliases(m,access,address,size,value,data):
        offset=address-0x50200000;kind=offset&0x3000
        if kind:
            original=address-kind;old=int.from_bytes(m.mem_read(original,size),'little')
            new=old^value if kind==0x1000 else old|value if kind==0x2000 else old&~value
            m.mem_write(original,new.to_bytes(size,'little'))
    uc.hook_add(UC_HOOK_MEM_WRITE,aliases,begin=0x50200000,end=0x50203fff)
    def call(name,*args):
        nonlocal returned
        returned=False
        for reg,v in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):uc.reg_write(reg,v)
        uc.reg_write(UC_ARM_REG_SP,image.symbols['__StackTop'][0]);uc.reg_write(UC_ARM_REG_LR,0x201)
        try:uc.emu_start(image.symbols[name][0]|1,0,count=3000000)
        except Exception as exc:raise AssertionError(f'{name} PC={uc.reg_read(UC_ARM_REG_PC):#x}, LR={uc.reg_read(UC_ARM_REG_LR):#x}') from exc
        assert returned,f'{name} stuck at {uc.reg_read(UC_ARM_REG_PC):#x}'
        return uc.reg_read(UC_ARM_REG_R0)
    def addr(name):return image.symbols[name][0]
    engine=addr('engine');bus=engine+fields['Engine.bus'];config=engine+fields['Engine.project']+fields['Project.sn_clock']
    def read(a,fmt='B'):return struct.unpack('<'+fmt,uc.mem_read(a,struct.calcsize('<'+fmt)))[0]
    def write(a,v,fmt='B'):uc.mem_write(a,struct.pack('<'+fmt,v))
    def field(base,name):return base+fields[name]
    def step(dt):
        nonlocal now
        now+=dt;write(0x40054028,now&0xffffffff,'I');call('sn_clock_control')
    def busy():return read(field(bus,'RegisterBus.busy'))!=0
    def state():return read(field(engine,'Engine.sn_clock_state'))
    write(addr('clock_sm'),0,'i');write(addr('rail_on'),1)
    call('sn_clock_start')
    assert read(0x50200000,'I')&1,'initial start failed to enable PIO'
    assert read(field(engine,'Engine.sn_clock_hz'),'d')==2000000
    for _ in range(500):step(100)
    assert not busy()
    # Low-frequency pending transaction survives a stop request and a divider pair.
    write(config+fields['SnClockConfig.base_hz'],100,'d');step(1000)
    call('bus_tone',bus,0,0,568)
    for _ in range(100):
        step(1000)
        if busy():break
    assert busy()
    write(config+fields['SnClockConfig.stopped'],1);step(1000)
    assert read(field(engine,'Engine.sn_stop_pending')) and read(0x50200000,'I')&1
    for _ in range(2000):
        step(1000)
        if state()==3:break
    assert state()==3 and not busy() and not(read(0x50200000,'I')&1)
    for _ in range(20):step(1000);assert not busy()
    # Recover at an experimental high clock, then panic while a byte is held.
    write(config+fields['SnClockConfig.base_hz'],8000000,'d');write(config+fields['SnClockConfig.stopped'],0)
    step(1000);assert state()==1 and read(0x50200000,'I')&1
    call('bus_tone',bus,1,2,777)
    for _ in range(100):
        step(1)
        if busy():break
    assert busy();call('engine_panic',engine,0xffffffff)
    for _ in range(500):step(20)
    assert not busy()
    assert bytes(uc.mem_read(field(bus,'RegisterBus.volume_sent'),8))==b'\x0f'*8
    # Two modulation slots, FULL clipping, and recovery stay callable under load.
    write(config+fields['SnClockConfig.follow'],2)
    mod=config+fields['SnClockConfig.mod'];write(mod+fields['SnModConfig.mode'],8)
    write(mod+fields['SnModConfig.depth'],300,'i');write(mod+fields['SnModConfig.rate_millihz'],100000,'I')
    for _ in range(100):step(250);call('engine_tick',engine,1)
    return {'firmware':'v0.11.0-alpha','elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'arm_clock_bus_glue_executed':True,'initial_pio_enable':True,'low_clock_transfer_then_stop':True,'no_new_transfer_while_stopped':True,'experimental_high_clock_resume':True,'panic_during_transfer':True,'full_modulation_callable':True,'hardware_tested':False,'limitations':['MMIO alias semantics modeled; PIO waveforms checked separately.','Time/ROM arithmetic are functional stubs; no device WCET, GPIO or IC verification.']}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();result=verify(a.elf);a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
