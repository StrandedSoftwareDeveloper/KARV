using Brutal.ImGuiApi;
using Brutal.Numerics;
using Brutal.VulkanApi;
using Brutal.VulkanApi.Abstractions;
using System.Runtime.InteropServices;
using KSA;
using StarMap.API;
//using StarMap.SimpleMod.Dependency;

namespace KARV
{
    [StarMapMod]
    public class KARV
    {
        public const uint TARGET_STEPS_PER_TICK = 65536*5;
        private bool on = true;
        
        struct stepRetVal {
            public int statusCode;
            public int kbBufferLen;
        };
        
        [DllImport("libkarv")]
        private static extern void setup(ushort width, ushort height);
        [DllImport("libkarv")]
        private static unsafe extern stepRetVal step(byte *buffer, char *kbBuffer, int len, uint targetSteps);
        [DllImport("libkarv")]
        private static extern void cleanup();
        
        //public ImTextureRef texID;
        Stack<char> keyboardBuffer;
        private byte[] vram;

        [StarMapAfterGui]
        public void OnAfterUi(double dt)
        {
            Console.WriteLine("Hi from KARV!");
            
            if (!on) {
                return;
            }
            
            stepRetVal ret;
            unsafe {
                fixed (byte *buffer = vram) {
                    fixed (char *kbBuffer = keyboardBuffer.ToArray()) {
                        ret = step(buffer, kbBuffer, keyboardBuffer.Count, TARGET_STEPS_PER_TICK);
                    }
                }
            }
            
            while (ret.kbBufferLen < keyboardBuffer.Count) {
                keyboardBuffer.Pop();
            }
            
            switch( ret.statusCode )
            {
                case 0: break;
                case 1: Console.WriteLine("Tried to sleep"); break;//if( do_sleep ) MiniSleep(); *this_ccount += instrs_per_flip; break;
                case 3: Console.WriteLine("Tried to reset instct"); break;//instct = 0; break;
                case 0x7777: Console.WriteLine("Tried to restart"); break;	//syscon code for restart
                case 0x5555: Console.WriteLine("POWEROFF"); on = false; break;//printf( "POWEROFF@0x%08x%08x\n", core->cycleh, core->cyclel ); running = 0; break; //syscon code for power-off
                default: Console.WriteLine( "Unknown failure" ); break;
            }
            
            /*ImGuiWindowFlags flags = ImGuiWindowFlags.MenuBar;

            ImGui.Begin("MyWindow", flags);
            if (ImGui.BeginMenuBar())
            {
                if (ImGui.BeginMenu("SimpleMod"))
                {
                    if (ImGui.MenuItem("Show Message"))
                    {
                        Console.WriteLine("Hello from SimpleMod!");
                    }
                    ImGui.EndMenu();
                }
                ImGui.EndMenuBar();
            }
            ImGui.Text("Hello from SimpleMod!");
            ImGui.End();

            ImGui.Begin("MyWindow", flags);
            if (ImGui.BeginMenuBar())
            {
                if (ImGui.BeginMenu("SimpleMod 2"))
                {
                    if (ImGui.MenuItem("Show Message 2"))
                    {
                        Console.WriteLine("Hello from SimpleMod 2!");
                    }
                    ImGui.EndMenu();
                }
                ImGui.EndMenuBar();
            }
            ImGui.Text("Hello from SimpleMod 2!");
            ImGui.End();

            flags = ImGuiWindowFlags.NoTitleBar | ImGuiWindowFlags.NoResize | ImGuiWindowFlags.NoMove | ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoBackground | ImGuiWindowFlags.NoSavedSettings | ImGuiWindowFlags.MenuBar;

            ImGui.Begin((ImString)"Menu Bar", flags);
            if (ImGui.BeginMenuBar())
            {
                if (ImGui.BeginMenu((ImString)"SimpleMod 2"))
                {
                    if (ImGui.MenuItem("Show Message 2"))
                    {
                        Console.WriteLine("Hello from SimpleMod 2!");
                    }
                    ImGui.EndMenu();
                }
                ImGui.EndMenuBar();
            }
            ImGui.Text("Hello from SimpleMod 2!");
            ImGui.End();*/

            /*ImGuiWindowFlags flags = ImGuiWindowFlags.None;
            ImGui.Begin((ImString)"ImGui Begin thing", flags);
            ImGui.Image(texID, new float2(200.0f, 100.0f), new float2(0.0f, 0.0f), new float2(1.0f, 1.0f));
            ImGui.End();*/
        }

        [StarMapBeforeMain]
        public void OnBeforeMain()
        {
            setup(400, 400);
            keyboardBuffer = new Stack<char>();
            
            vram = new byte[400*400*4];
            
            Console.WriteLine($"SimpleMod - On before main loaded!");
        }

        [StarMapImmediateLoad]
        public void OnImmediateLoad(Mod mod)
        {
            /*var texture = new RenderCore.TextureAsset(
                "Content/Core/Textures/Io_Diffuse.jpg",
                new(new Brutal.StbApi.Texture.StbTexture.LoadSettings { ForceChannels = 4 }));

            
            var renderer = KSA.Program.GetRenderer();
            RenderCore.SimpleVkTexture vkTex;
            
            using (var stagingPool = renderer.Device.CreateStagingPool(renderer.Graphics, 1)) {
                vkTex = new RenderCore.SimpleVkTexture(renderer.Device, stagingPool, texture);
            }

            var sampler = renderer.Device.CreateSampler(Presets.Sampler.SamplerPointClamped, null);
            texID = ImGuiBackend.Vulkan.AddTexture(sampler, vkTex.ImageView);*/
            Console.WriteLine($"SimpleMod - On immediate loaded, modname: {mod.Name}");
        }

        /*[StarMapAllModsLoaded]
        public void OnFullyLoaded()
        {
            var @object = new DependencyClass();
            Console.WriteLine("SimpleMod - On fully loaded");
            Patcher.Patch();
        }*/

        [StarMapUnload]
        public void Unload()
        {
            Console.WriteLine("SimpleMod - Unload");
            //Patcher.Unload();
        }
    }
}