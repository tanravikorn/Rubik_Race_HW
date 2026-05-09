`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 05/09/2026 08:14:45 PM
// Design Name: 
// Module Name: camera_capture
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


module camera_capture(
       input pclk,
       input vsync,
       input href,
       input [7:0] d,
       output reg [16:0] addr = 0,
       output reg [15:0] dout = 0,
       output reg we = 0
    );
    
    reg [15:0] temp = 0;
    reg write_state = 0;
    reg vsync_d = 0;
    reg frame_full = 0;
    localparam [16:0] FRAME_PIXELS = 17'd76800;
    
    always @(posedge pclk) begin
        vsync_d <= vsync;
        we <= 0;

        if(vsync && !vsync_d) begin
            addr <= 0;
            write_state <= 0;
            frame_full <= 0;
        end else begin
            if(href && !frame_full) begin
                if(write_state == 0) begin
                    temp <= {d, 8'h00};
                    write_state <= 1;
                end else begin
                    dout <= {temp[15:8], d};
                    we <= 1;
                    write_state <= 0;
                    if(addr == FRAME_PIXELS - 1) begin
                        frame_full <= 1;
                    end else begin
                        addr <= addr + 1;
                    end
                end
            end else begin
                write_state <= 0;
            end
        end
    end 
endmodule
