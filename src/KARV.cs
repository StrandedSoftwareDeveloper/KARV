/*----------------------------------------------------------------------------*\
 | KARV.cs Copyright (c) 2025 StrandedSoftwareDeveloper under the MIT License |
 | The main file of KARV, responsibilities include:                           |
 |  -KARVComputer PartModule                                                  |
 |  -Calling libkarv                                                          |
 |  -Presentation of the final framebuffer                                    |
 |  -Passing keyboard inputs to libkarv                                       |
 |  -All UI tasks                                                             |
 |  -(Future) Vessel data/control interface                                   |
\*----------------------------------------------------------------------------*/

using System.Runtime.InteropServices;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;

namespace KARV
{
    public class KARVComputer : PartModule
    {
        struct stepRetVal {
            public int statusCode;
            public int kbBufferLen;
        };
        
        [KSPField(guiActive = true, guiActiveEditor = true, guiName = "Terminal"), UI_Toggle(enabledText = "Hide terminal", disabledText = "Show terminal")]
        public bool showTerminal = false;

        private bool on = true;
        private byte[] vram;
        Stack<char> keyboardBuffer;
        GameObject uiImageObject;
        UnityEngine.UI.RawImage fbUIRawImage;
        bool initialized = false;
        bool dragging = false;
        Vector2 dragOffset = Vector3.zero;
        RectTransform rectTransform;

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
            fbUIRawImage = uiImageObject.AddComponent<UnityEngine.UI.RawImage>();
            fbUIRawImage.texture = fbTex;
            fbUIRawImage.enabled = showTerminal;
            fbUIRawImage.SetAllDirty();
            rectTransform = uiImageObject.GetComponent<RectTransform>();
            rectTransform.SetParent(MainCanvasUtil.MainCanvas.transform);
            rectTransform.localScale *= 4;
            rectTransform.Rotate(180.0f, 0.0f, 0.0f);
            //rectTransform.localScale.y *= -1;
            uiImageObject.SetActive(true);

            keyboardBuffer = new Stack<char>();

            vram = new byte[fbTex.width*fbTex.height*4];
            
            initialized = true;
        }

        public void OnGUI() {
            if (!showTerminal) {
                return;
            }
            
            Event ev = Event.current;
            
            if (ev.isMouse) {
                if (ev.type == EventType.MouseDown) {
                    //Debug.Log("Positions:");
                    //Debug.Log(rectTransform.localPosition);
                    //Debug.Log(rectTransform.position);
                    //Debug.Log(ev.mousePosition);
                    //if (clickedOnTerminal(ev.mousePosition, rectTransform.position, new Vector2(400.0f, 400.0f))) {
                    if (true) { //That's broken ^
                        dragging = true;
                        dragOffset = ev.mousePosition;
                        dragOffset.x = rectTransform.localPosition.x - dragOffset.x;
                        dragOffset.y = rectTransform.localPosition.y - (-dragOffset.y);
                    }
                } else if (ev.type == EventType.MouseUp) {
                    dragging = false;
                }
                
                if (dragging) {
                    Vector3 newPos = ev.mousePosition;
                    newPos.y = -newPos.y;
                    newPos.x += dragOffset.x;
                    newPos.y += dragOffset.y;
                    rectTransform.localPosition = newPos;
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
            fbUIRawImage.enabled = showTerminal;
            if (showTerminal) {
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
        
        bool clickedOnTerminal(Vector2 testPos, Vector3 boxPos, Vector2 boxSize) {
            Vector2 offset = new Vector2(1600.0f/2.0f + boxSize.x, 900.0f/2.0f);
            return testPos.x >= boxPos.x && testPos.x + offset.x < boxPos.x + boxSize.x + offset.x &&
                   testPos.y >= (-boxPos.y) + offset.y && testPos.y < (-boxPos.y) + boxSize.y + offset.y;
        }
    }
}
