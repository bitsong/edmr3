/*!
 * File generated by platform wizard. DO NOT MODIFY
 *
 */

metaonly module Platform inherits xdc.platform.IPlatform {

    config ti.platforms.generic.Platform.Instance CPU =
        ti.platforms.generic.Platform.create("CPU", {
            clockRate:      456.0,                                       
            catalogName:    "ti.catalog.c6000",
            deviceName:     "OMAPL138",
            customMemoryMap:
           [          
                ["SR_0", 
                     {
                        name: "SR_0",
                        base: 0xC2001000,                    
                        len: 0x100000,                    
                        space: "data",
                        access: "RWX",
                     }
                ],
                ["SR_1", 
                     {
                        name: "SR_1",
                        base: 0xC2101000,                    
                        len: 0x400000,                    
                        space: "data",
                        access: "RWX",
                     }
                ],
                ["SR_2", 
                     {
                        name: "SR_2",
                        base: 0xC2501000,                    
                        len: 0x3FE000,                    
                        space: "data",
                        access: "RWX",
                     }
                ],
                ["SR_3", 
                     {
                        name: "SR_3",
                        base: 0xC28FF000,                    
                        len: 0x700000,                    
                        space: "data",
                        access: "RWX",
                     }
                ],
                ["DSP_PROG", 
                     {
                        name: "DSP_PROG",
                        base: 0xC3000000,                    
                        len: 0xFFF000,                    
                        space: "code/data",
                        access: "RWX",
                     }
                ],
                ["IROM", 
                     {
                        name: "IROM",
                        base: 0x11700000,                    
                        len: 0x00100000,                    
                        space: "code/data",
                        access: "RX",
                     }
                ],
                ["IRAM", 
                     {
                        name: "IRAM",
                        base: 0x11800000,                    
                        len: 0x00040000,                    
                        space: "code/data",
                        access: "RWX",
                     }
                ],
                ["L1DSRAM", 
                     {
                        name: "L1DSRAM",
                        base: 0x11f00000,                    
                        len: 0x00008000,                    
                        space: "data",
                        access: "RW",
                     }
                ],
                ["L1PSRAM", 
                     {
                        name: "L1PSRAM",
                        base: 0x11e00000,                    
                        len: 0x00008000,                    
                        space: "code",
                        access: "RWX",
                     }
                ],
                ["L3_CBA_RAM", 
                     {
                        name: "L3_CBA_RAM",
                        base: 0x80000000,                    
                        len: 0x00020000,                    
                        space: "code/data",
                        access: "RWX",
                     }
                ],
           ],
          l2Mode: "32k",
          l1PMode: "32k",
          l1DMode: "32k",

    });
    
instance :
    
    override config string codeMemory  = "DSP_PROG";   
    override config string dataMemory  = "DSP_PROG";                                
    override config string stackMemory = "DSP_PROG";

    config String l2Mode = "32k";
    config String l1PMode = "32k";
    config String l1DMode = "32k";
}