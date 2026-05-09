`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 05/09/2026 05:45:59 PM
// Design Name: 
// Module Name: camera_config
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module camera_config(
        input clk,
        input reset,
        input sccb_ready,
        output reg sccb_start = 0,
        output [7:0] reg_addr,
        output [7:0] reg_data,
        output reg done = 0
    );
    
    reg [7:0] rom_addr = 0;
    reg [15:0] rom_data;
    reg [24:0] reset_wait_cnt = 0;
    localparam [24:0] RESET_WAIT_CYCLES = 25'd30_000_000;
    
    always @(*) begin
        case(rom_addr)
            8'd0:  rom_data = 16'h1280; // COM7: Reset
            
            // แก้ตรงนี้: เปลี่ยน 1214 เป็น 1216 เพื่อเปิดโหมด Color Bar
            8'd1:  rom_data = 16'h1216; // COM7: RGB + QVGA + Color Bar Test Pattern
            
            8'd2:  rom_data = 16'h40D0; // COM15: RGB565
            8'd3:  rom_data = 16'h1101; // CLKRC: หาร 4
            8'd4:  rom_data = 16'h3A00; // TSLB: ปิด Byte Swap
            8'd5:  rom_data = 16'h1500; // COM10
            8'd6:  rom_data = 16'h0C0C; // COM3: Enable Scaling
            8'd7:  rom_data = 16'h3E19; // COM14: Scaling PCLK divide
            8'd8:  rom_data = 16'h8C00; // RGB444 disable
            
            // เพิ่มตรงนี้: เปิด DSP Color Bar
            8'd9:  rom_data = 16'h4208; // COM17: DSP Color Bar Enable
            
            8'd10: rom_data = 16'hFFFF; // จบการทำงาน
            default: rom_data = 16'hFFFF;
        endcase
    end
    
    assign reg_addr  = rom_data[15:8];
    assign reg_data = rom_data[7:0];

    reg [1:0] state = 0;
    always @(posedge clk) begin
        if(reset) begin
            rom_addr <= 0;
            state <= 0;
            reset_wait_cnt <= 0;
            sccb_start <= 0;
            done <= 0;
        end else begin
            case(state)
                0: begin
                    sccb_start <= 0;
                    if(rom_data == 16'hFFFF) begin
                        done <= 1;
                    end else if(sccb_ready) begin
                        sccb_start <= 1;
                        state <= 1;
                    end
                end
                1: begin
                    if(!sccb_ready) begin
                        sccb_start <= 0;
                        state <= 2;
                    end
                end
                2: begin
                    if(sccb_ready) begin
                        if(rom_addr == 0) begin
                            reset_wait_cnt <= 0;
                            state <= 3;
                        end else begin
                            rom_addr <= rom_addr + 1;
                            state <= 0;
                        end
                    end
                end
                3: begin
                    if(reset_wait_cnt == RESET_WAIT_CYCLES - 1) begin
                        rom_addr <= rom_addr + 1;
                        state <= 0;
                    end else begin
                        reset_wait_cnt <= reset_wait_cnt + 1;
                    end
                end
            endcase
             
       end
        
    end
     
endmodule
