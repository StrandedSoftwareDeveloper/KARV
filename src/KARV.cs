using System.Runtime.InteropServices;
using System.Collections.Generic;
using UnityEngine;

namespace KARV
{
    public class KARVComputer : PartModule
    {
        struct stepRetVal {
            public int statusCode;
            public int kbBufferLen;
        };

        private bool on = true;
        private byte[] vram;
        Stack<char> keyboardBuffer;
        GameObject uiImageObject;
        bool initialized = false;

        [DllImport("libkarv")]
        private static extern void setup(ushort width, ushort height);
        [DllImport("libkarv")]
        private static unsafe extern stepRetVal step(byte *buffer, char *kbBuffer, int len);
        [DllImport("libkarv")]
        private static extern void cleanup();

        public override void OnInitialize()
        {
            Debug.Log("setup");
            setup(400, 400);

            UnityEngine.Texture2D fbTex = new UnityEngine.Texture2D(400, 400, TextureFormat.RGBA32, false); //FramebufferTexture
            var fbData = fbTex.GetRawTextureData<Color32>();
            Color32 color = new Color32(0, 0, 0, 255);
            for (int i=fbTex.width*fbTex.height-1; i>=0; i--) {
                color.r = 0;
                color.g = 0;
                color.b = 0;
                color.a = 255;
                fbData[i] = color;
            }
            fbTex.Apply(false, false);

            uiImageObject = new GameObject("framebufferUIImageObject");
            UnityEngine.UI.RawImage fbUIRawImage = uiImageObject.AddComponent<UnityEngine.UI.RawImage>();
            fbUIRawImage.texture = fbTex;
            fbUIRawImage.enabled = true;
            fbUIRawImage.SetAllDirty();
            RectTransform transform = uiImageObject.GetComponent<RectTransform>();
            transform.SetParent(MainCanvasUtil.MainCanvas.transform);
            transform.localScale *= 4;
            transform.Rotate(180.0f, 0.0f, 0.0f);
            transform.Translate(Random.Range(-150.0f, 150.0f), Random.Range(-150.0f, 150.0f), 0.0f);
            //transform.localScale.y *= -1;
            uiImageObject.SetActive(true);

            keyboardBuffer = new Stack<char>();

            vram = new byte[fbTex.width*fbTex.height*4];
            
            initialized = true;
        }

        public void OnGUI() {
            Event ev = Event.current;
            if (ev.isMouse) {
                if (ev.type == EventType.MouseDrag) {
                    RectTransform transform = uiImageObject.GetComponent<RectTransform>();
                    transform.localPosition = ev.mousePosition;
                }
            }
            if (ev.isKey && ev.type == EventType.KeyDown) {
                if (ev.character >= ' ' && ev.character <= '~') {
                    Debug.Log("KARV: OnGUI \'" + ev.character + "\'");
                    keyboardBuffer.Push(ev.character);
                } else if (ev.keyCode == KeyCode.Return) {
                    keyboardBuffer.Push('\n');
                } else if (ev.keyCode == KeyCode.Backspace) {
                    keyboardBuffer.Push((char)127); //127 is DEL
                }
            }
        }

        public override void OnStart(StartState state)
        {
            Debug.Log("KARV: OnStart");
            Debug.Log(state.ToString());
            Debug.Log(this.isActiveAndEnabled.ToString());

            //setup();
        }

        public void Update() {
            UnityEngine.Texture2D fbTex = (Texture2D)GameObject.Find("framebufferUIImageObject").GetComponent<UnityEngine.UI.RawImage>().texture;
            /*var fbData = fbTex.GetRawTextureData<Color32>();
            Color32 color = new Color32(0, 0, 0, 255);
            for (int y=0; y<fbTex.height; y++) {
                for (int x=0; x<fbTex.width; x++) {
                    color.r = vram[(y*fbTex.width+x)*4+0];
                    color.g = vram[(y*fbTex.width+x)*4+1];
                    color.b = vram[(y*fbTex.width+x)*4+2];
                    color.a = vram[(y*fbTex.width+x)*4+3];
                    fbData[(fbTex.height-y)*fbTex.width+x] = color;
                }
            }*/
            //fbTex.SetPixelData<Color32>(fbData, 0);
            fbTex.SetPixelData(vram, 0, 0);
            fbTex.Apply(false, false);
        }

        public void FixedUpdate() {
            //Debug.Log("KARV: FixedUpdate");
            if (on) {
                stepRetVal ret;
                unsafe {
                    fixed (byte *buffer = vram) {
                        fixed (char *kbBuffer = keyboardBuffer.ToArray()) {
                            ret = step(buffer, kbBuffer, keyboardBuffer.Count);
                        }
                    }
                }

                while (ret.kbBufferLen < keyboardBuffer.Count) {
                    keyboardBuffer.Pop();
                }

                switch( ret.statusCode )
                {
                    case 0: break;
                    case 1: Debug.Log("Tried to sleep"); break;//if( do_sleep ) MiniSleep(); *this_ccount += instrs_per_flip; break;
                    case 3: Debug.Log("Tried to reset instct"); break;//instct = 0; break;
                    case 0x7777: Debug.Log("Tried to restart"); break;	//syscon code for restart
                    case 0x5555: Debug.Log("POWEROFF"); on = false; break;//printf( "POWEROFF@0x%08x%08x\n", core->cycleh, core->cyclel ); running = 0; break; //syscon code for power-off
                    default: Debug.Log( "Unknown failure" ); break;
                }
            }
        }

        public override void OnInactive() {
            Debug.Log("KARV: OnInactive");
        }
        
        public void OnDisable() {
            Debug.Log("KARV: OnDisable");
        }
        
        public void OnDestroy() {
            Debug.Log("KARV: OnDestroy");
            
            if (initialized) {
                uiImageObject.DestroyGameObject();
                cleanup();
                initialized = false;
            }
        }
    }
}
