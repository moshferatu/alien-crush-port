# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $ReferenceDirectory,
    [Parameter(Mandatory)][string] $MesenDirectory
)
$ErrorActionPreference='Stop'
# Compile the byte comparison rather than performing millions of PowerShell operations.
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Security.Cryptography;
using System.Collections.Generic;
public static class AlienMesenComparison {
    static string Hash(SHA256 sha, byte[] bytes) { return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant(); }
    public static string Compare(string reference, string mesen) {
        var census=File.ReadAllLines(Path.Combine(reference,"census.csv"));
        var own=File.ReadAllBytes(Path.Combine(reference,"work-ram.bin"));
        var other=File.ReadAllBytes(Path.Combine(mesen,"work-ram.bin"));
        var pixels=File.ReadAllBytes(Path.Combine(mesen,"pixels.rgb"));
        var ownPixels=File.ReadAllBytes(Path.Combine(reference,"pixels.argb"));
        int width=BitConverter.ToInt32(pixels,4), height=BitConverter.ToInt32(pixels,8), frames=BitConverter.ToInt32(pixels,12);
        if(Encoding.ASCII.GetString(pixels,0,4)!="ACF1" || width!=292 || height!=242) throw new Exception("Unexpected Mesen video geometry; establish normalization first");
        if(census.Length!=frames+1 || own.Length!=frames*8192 || other.Length!=own.Length || pixels.Length!=16+frames*width*height*3 || ownPixels.Length!=frames*256*240*4) throw new Exception("Capture lengths disagree");
        var counts=new int[8192]; var first=new int[8192]; var last=new int[8192];
        int pixelFrames=0,ramFrames=0;
        using(var sha=SHA256.Create()) for(int f=0;f<frames;f++) {
            bool same=true;
            for(int i=0;i<8192;i++) if(own[f*8192+i]!=other[f*8192+i]) {
                if(counts[i]++==0) first[i]=f+1; last[i]=f+1; same=false;
            }
            if(same) ramFrames++;
            // Mesen's 242-row surface contains only 239 rows after the verified
            // y=3 alignment. Compare that shared region; never invent a black row.
            var normalized=new byte[256*239*4];
            var expected=new byte[normalized.Length];
            Array.Copy(ownPixels,f*256*240*4,expected,0,expected.Length);
            for(int y=0;y<239;y++) for(int x=0;x<256;x++) {
                int to=(y*256+x)*4; normalized[to+3]=255;
                if(y<239) {
                    int from=16+((f*height+y+3)*width+x+18)*3;
                    normalized[to]=pixels[from+2]; normalized[to+1]=pixels[from+1]; normalized[to+2]=pixels[from];
                }
            }
            if(Hash(sha,normalized)==Hash(sha,expected)) pixelFrames++;
        }
        var report=new StringBuilder("offset,different_frames,first_frame,last_frame\n");
        for(int i=0;i<8192;i++) if(counts[i]>0) report.AppendFormat("0x{0:X4},{1},{2},{3}\n",i,counts[i],first[i],last[i]);
        File.WriteAllText(Path.Combine(mesen,"ram-differences.csv"),report.ToString());
        return String.Format("Frames: {0}; exact RAM frames: {1}; exact shared 256x239 pixel frames: {2}. See ram-differences.csv; differences require classification. The final reference scanline lies outside Mesen's capture.",frames,ramFrames,pixelFrames);
    }
}
'@
[AlienMesenComparison]::Compare([System.IO.Path]::GetFullPath($ReferenceDirectory),[System.IO.Path]::GetFullPath($MesenDirectory))
