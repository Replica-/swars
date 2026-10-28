// Records what the computer plays (the "loopback" of the default output device, through the Windows Core Audio API)
// into a WAV file; used by record-mission.ps1, as ffmpeg has no input for it. While nothing is played the device gives no
// data, so the gaps are filled with silence, to keep the recording in step with the time.
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

public class LoopbackRecorder
{
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    class MMDeviceEnumeratorComObject { }

    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDeviceEnumerator
    {
        int EnumAudioEndpoints(int dataFlow, int stateMask, out IntPtr devices);
        int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice endpoint);
    }

    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDevice
    {
        int Activate(ref Guid iid, int clsCtx, IntPtr activationParams, [MarshalAs(UnmanagedType.IUnknown)] out object iface);
    }

    [ComImport, Guid("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioClient
    {
        int Initialize(int shareMode, uint streamFlags, long bufferDuration, long periodicity, IntPtr format, IntPtr sessionGuid);
        int GetBufferSize(out uint frames);
        int GetStreamLatency(out long latency);
        int GetCurrentPadding(out uint padding);
        int IsFormatSupported(int shareMode, IntPtr format, out IntPtr closest);
        int GetMixFormat(out IntPtr format);
        int GetDevicePeriod(out long defaultPeriod, out long minimumPeriod);
        int Start();
        int Stop();
        int Reset();
        int SetEventHandle(IntPtr handle);
        int GetService(ref Guid iid, [MarshalAs(UnmanagedType.IUnknown)] out object service);
    }

    [ComImport, Guid("C8ADBD64-E71E-48a0-A4DE-185C395CD317"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioCaptureClient
    {
        int GetBuffer(out IntPtr data, out uint frames, out uint flags, out ulong devicePosition, out ulong qpcPosition);
        int ReleaseBuffer(uint frames);
        int GetNextPacketSize(out uint frames);
    }

    const uint AUDCLNT_STREAMFLAGS_LOOPBACK = 0x00020000;
    const uint AUDCLNT_BUFFERFLAGS_SILENT = 0x2;

    Thread thread;
    volatile bool stop;
    string path;
    public string Error = null;
    /// <summary>Milliseconds on the stopwatch given to Start at which the first data of the recording belongs.</summary>
    public long StartMs = 0;

    public void Start(string wavPath, Stopwatch clock)
    {
        path = wavPath;
        stop = false;
        thread = new Thread(() => Run(clock));
        thread.IsBackground = true;
        thread.SetApartmentState(ApartmentState.MTA);
        thread.Start();
        // Wait until the capture runs, or fails
        for (int i = 0; i < 100 && !started && Error == null; i++)
            Thread.Sleep(20);
    }

    volatile bool started;

    public void Stop()
    {
        stop = true;
        if (thread != null)
            thread.Join(5000);
    }

    void Run(Stopwatch clock)
    {
        FileStream fs = null;
        try
        {
            var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumeratorComObject();
            IMMDevice device;
            Marshal.ThrowExceptionForHR(enumerator.GetDefaultAudioEndpoint(0 /* render */, 0 /* console */, out device));
            Guid iidClient = typeof(IAudioClient).GUID;
            object o;
            Marshal.ThrowExceptionForHR(device.Activate(ref iidClient, 23 /* all contexts */, IntPtr.Zero, out o));
            var client = (IAudioClient)o;

            IntPtr pFormat;
            Marshal.ThrowExceptionForHR(client.GetMixFormat(out pFormat));
            ushort tag = (ushort)Marshal.ReadInt16(pFormat, 0);
            ushort channels = (ushort)Marshal.ReadInt16(pFormat, 2);
            int rate = Marshal.ReadInt32(pFormat, 4);
            ushort blockAlign = (ushort)Marshal.ReadInt16(pFormat, 12);
            ushort bits = (ushort)Marshal.ReadInt16(pFormat, 14);
            // The mix format is nearly always 32 bit float, as an "extensible" format; 3 is the tag of float in a WAV
            ushort wavTag = (bits == 32 && (tag == 3 || tag == 0xFFFE)) ? (ushort)3 : (ushort)1;

            // 1 s buffer, in units of 100 ns
            Marshal.ThrowExceptionForHR(client.Initialize(0 /* shared */, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, pFormat, IntPtr.Zero));
            Guid iidCapture = typeof(IAudioCaptureClient).GUID;
            Marshal.ThrowExceptionForHR(client.GetService(ref iidCapture, out o));
            var capture = (IAudioCaptureClient)o;

            fs = new FileStream(path, FileMode.Create, FileAccess.Write);
            var bw = new BinaryWriter(fs);
            WriteHeader(bw, wavTag, channels, rate, bits, 0);

            long framesWritten = 0;
            byte[] silence = new byte[blockAlign * 4800];
            long startMs = clock.ElapsedMilliseconds;
            StartMs = startMs;
            Marshal.ThrowExceptionForHR(client.Start());
            started = true;

            while (!stop)
            {
                Thread.Sleep(5);
                uint packet;
                while (!stop && capture.GetNextPacketSize(out packet) == 0 && packet > 0)
                {
                    IntPtr data;
                    uint frames, flags;
                    ulong devPos, qpcPos;
                    if (capture.GetBuffer(out data, out frames, out flags, out devPos, out qpcPos) != 0)
                        break;
                    // The data is what was played just now; if the recording is behind the time, the device was silent
                    // before it, and that is put in
                    long wanted = (clock.ElapsedMilliseconds - startMs) * rate / 1000 - frames;
                    framesWritten += WriteSilence(bw, silence, blockAlign, wanted - framesWritten, 0.03 * rate);
                    byte[] buf = new byte[frames * blockAlign];
                    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)
                        Marshal.Copy(data, buf, 0, buf.Length);
                    bw.Write(buf);
                    framesWritten += frames;
                    capture.ReleaseBuffer(frames);
                }
                // Long quiet: keep the file in step with the time
                long now = (clock.ElapsedMilliseconds - startMs) * rate / 1000;
                if (now - framesWritten > rate / 2)
                    framesWritten += WriteSilence(bw, silence, blockAlign, now - framesWritten - rate / 10, 0);
            }
            client.Stop();
            long end = (clock.ElapsedMilliseconds - startMs) * rate / 1000;
            framesWritten += WriteSilence(bw, silence, blockAlign, end - framesWritten, 0);
            bw.Flush();
            WriteHeader(bw, wavTag, channels, rate, bits, (uint)(framesWritten * blockAlign));
            bw.Flush();
        }
        catch (Exception e)
        {
            Error = e.Message;
        }
        finally
        {
            if (fs != null)
                fs.Close();
        }
    }

    static long WriteSilence(BinaryWriter bw, byte[] silence, int blockAlign, long frames, double tolerance)
    {
        if (frames <= tolerance)
            return 0;
        long left = frames;
        while (left > 0)
        {
            long n = Math.Min(left, silence.Length / blockAlign);
            bw.Write(silence, 0, (int)(n * blockAlign));
            left -= n;
        }
        return frames;
    }

    static void WriteHeader(BinaryWriter bw, ushort tag, ushort channels, int rate, ushort bits, uint dataBytes)
    {
        bw.Seek(0, SeekOrigin.Begin);
        bw.Write(new[] { 'R', 'I', 'F', 'F' });
        bw.Write(36 + dataBytes);
        bw.Write(new[] { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
        bw.Write(16);
        bw.Write(tag);
        bw.Write(channels);
        bw.Write(rate);
        bw.Write(rate * channels * bits / 8);
        bw.Write((ushort)(channels * bits / 8));
        bw.Write(bits);
        bw.Write(new[] { 'd', 'a', 't', 'a' });
        bw.Write(dataBytes);
        bw.Seek(0, SeekOrigin.End);
    }
}


/// <summary>Notes on the clock when ffmpeg starts to record the picture (it says "Stream mapping" then), to line up the
/// sound with it.</summary>
public class FfmpegWatch
{
    public long VideoStartMs = -1;

    public void Attach(Process p, Stopwatch clock)
    {
        p.ErrorDataReceived += (s, e) =>
        {
            if (e.Data != null && VideoStartMs < 0 && e.Data.Contains("Stream mapping"))
                VideoStartMs = clock.ElapsedMilliseconds;
        };
        p.BeginErrorReadLine();
    }
}