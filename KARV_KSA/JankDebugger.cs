using HarmonyLib;
using System.Reflection.Emit;

static class JankDebugger {
    public static List<CodeInstruction> instructionsCopy;
    public static bool stepMode = false;
    public static int stepCount = 0;

    public static void Inject(List<CodeInstruction> codes) {
        instructionsCopy = new List<CodeInstruction>(codes);

        //Console.WriteLine(codes.Count);
        int index = 1;
        int numConstraned = 0;
        for (int i = 0; i < instructionsCopy.Count-numConstraned-1; i++) {
            if (codes[index-1].opcode == OpCodes.Constrained) {
                index += 1;
                numConstraned += 1;
                continue;
            }
            codes.Insert(index, new CodeInstruction(OpCodes.Ldc_I4, i+1));
            codes.Insert(index+1, new CodeInstruction(OpCodes.Call, typeof(JankDebugger).GetMethod(nameof(DebuggerFn))));
            index += 3;
        }

        for (int i = 0; i < codes.Count; i++) {
            //Console.WriteLine(i + ":" + codes[i].opcode + " " + codes[i].operand);
        }
    }

    public static void DebuggerFn(int instructionIndex) {
        if (!stepMode) {
            return;
        }

        if (stepCount > 1) {
            stepCount -= 1;
            return;
        }

        Console.Write(instructionIndex + ": ");
        PrintInstruction(instructionsCopy[instructionIndex]);

        while (true) {
            Console.Write("> ");
            string? command = Console.ReadLine();

            if (command == null || command.Length == 0) {
                continue;
            }

            string[] tokens = command.Split(" ");
            switch (tokens[0]) {
                case "l":
                case "list": {
                    int num = 5;
                    if (tokens.Length > 1) {
                        if (!int.TryParse(tokens[1], out num)) {
                            Console.WriteLine("Usage: list [number_of_instructions_to_list]");
                            break;
                        }
                    }
                    
                    for (int i=0; i<num; i++) {
                        Console.Write((instructionIndex + i) + ": ");
                        PrintInstruction(instructionsCopy[instructionIndex + i]);
                    }

                    break;
                }

                case "c":
                case "continue": {
                    stepMode = false;
                    return;
                }

                case "n":
                case "next": {
                    if (tokens.Length > 1) {
                        int num = 0;
                        if (!int.TryParse(tokens[1], out num)) {
                            Console.WriteLine("Usage: next [number_of_instructions_to_run]");
                            break;
                        }
                        stepCount = num;
                    }

                    stepMode = true;
                    return;
                }

                default: {
                    Console.WriteLine("Unknown command \"" + tokens[0] + "\"");
                    break;
                }
            }
        }
    }

    public static void PrintInstruction(CodeInstruction inst) {
        Console.WriteLine(inst.opcode + " " + inst.operand);
    }
}