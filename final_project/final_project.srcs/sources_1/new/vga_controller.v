`timescale 1ns / 1ps

module vga_controller(
    input clk_vga,   
    output reg hsync,
    output reg vsync,
    output reg [3:0] vga_r,
    output reg [3:0] vga_g,
    output reg [3:0] vga_b,
    output reg [16:0] read_addr,
    input [15:0] frame_data
    );

    reg [9:0] h_cnt = 0;
    reg [9:0] v_cnt = 0;

    // ── Counter ───────────────────────────────────────────────────────────────
    always @(posedge clk_vga) begin
        if (h_cnt == 799) begin
            h_cnt <= 0;
            if (v_cnt == 524) v_cnt <= 0;
            else v_cnt <= v_cnt + 1;
        end else begin
            h_cnt <= h_cnt + 1;
        end
    end

    // ── Sync signals ──────────────────────────────────────────────────────────
    always @(posedge clk_vga) begin
        hsync <= (h_cnt >= 656 && h_cnt < 752) ? 0 : 1;
        vsync <= (v_cnt >= 490 && v_cnt < 492) ? 0 : 1;
    end

    // ── Address calculation ───────────────────────────────────────────────────
    // 320 = 256 + 64 = (1<<8) + (1<<6) - ใช้ shift+add แทน * เพื่อ timing
    wire [8:0] row = v_cnt[9:1];   // v_cnt / 2  (0..239)
    wire [8:0] col = h_cnt[9:1];   // h_cnt / 2  (0..319)
    wire [16:0] row_addr = ({8'b0, row} << 8) + ({8'b0, row} << 6); // row * 320

    always @(posedge clk_vga) begin
        if (h_cnt < 640 && v_cnt < 480)
            read_addr <= row_addr + {8'b0, col};
    end

    // ── Pixel output (1 cycle หลัง addr เพื่อรอ BRAM latency) ─────────────────
    // h_cnt > 0 เพื่อชดเชย 1 cycle latency ของ BRAM
    always @(posedge clk_vga) begin
        if (h_cnt > 0 && h_cnt <= 640 && v_cnt < 480) begin
            // RGB565: [15:11]=R, [10:5]=G, [4:0]=B
            // Basys3 VGA มี 4 bits ต่อ channel → เอา 4 bits บนสุด
            // คอมเมนต์การดึงข้อมูลจาก RAM ทิ้งไปก่อน
             vga_r <= frame_data[15:12];
             vga_g <= frame_data[10:7];
             vga_b <= frame_data[4:1];

//            vga_r <= read_addr[5:2]; 
//            vga_g <= read_addr[5:2];
//            vga_b <= read_addr[5:2];
        end else begin
            vga_r <= 0;
            vga_g <= 0;
            vga_b <= 0;
        end
    end

endmodule