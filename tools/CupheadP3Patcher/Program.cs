using System.Security.Cryptography;
using Mono.Cecil;
using Mono.Cecil.Cil;

static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length != 2)
        {
            Console.Error.WriteLine("Usage: CupheadP3Patcher <Assembly-CSharp-WiiU.dll> <output.dll>");
            return 2;
        }

        var input = Path.GetFullPath(args[0]);
        var output = Path.GetFullPath(args[1]);

        if (!File.Exists(input))
        {
            Console.Error.WriteLine($"Input not found: {input}");
            return 2;
        }

        try
        {
            Console.WriteLine($"Input : {input}");
            Console.WriteLine($"SHA256: {Sha256(input)}");

            using var module = ModuleDefinition.ReadModule(input, new ReaderParameters {
                InMemory = true,
                ReadWrite = false,
                ReadingMode = ReadingMode.Immediate
            });

            var playerId = RequireType(module, "PlayerId");
            var inputType = RequireType(module, "Input");
            var playerManager = RequireType(module, "PlayerManager");
            var level = RequireType(module, "Level");

            AddPlayerThreeEnum(playerId);
            var multiplayer2 = AddMultiplayer2(playerManager);

            PatchInputStart(inputType);
            PatchPlayerManagerCctor(playerManager, playerId, multiplayer2);
            PatchPlayerManagerAwake(playerManager, multiplayer2);
            PatchLevelCreatePlayers(level);

            Directory.CreateDirectory(Path.GetDirectoryName(output)!);
            module.Write(output, new WriterParameters());

            Console.WriteLine($"Output: {output}");
            Console.WriteLine($"SHA256: {Sha256(output)}");
            Console.WriteLine("Cuphead P3 structural prototype patch completed.");
            Console.WriteLine("NOTE: this build prepares P3 data structures; join/input behavior is patched in the next stage.");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("PATCH FAILED");
            Console.Error.WriteLine(ex);
            return 1;
        }
    }

    private static string Sha256(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
    }

    private static IEnumerable<TypeDefinition> AllTypes(ModuleDefinition module)
    {
        foreach (var type in module.Types)
        {
            foreach (var item in AllTypes(type))
                yield return item;
        }
    }

    private static IEnumerable<TypeDefinition> AllTypes(TypeDefinition type)
    {
        yield return type;
        foreach (var nested in type.NestedTypes)
        {
            foreach (var item in AllTypes(nested))
                yield return item;
        }
    }

    private static TypeDefinition RequireType(ModuleDefinition module, string name) =>
        AllTypes(module).FirstOrDefault(t => t.Name == name)
        ?? throw new InvalidOperationException($"Required type not found: {name}");

    private static FieldDefinition RequireField(TypeDefinition type, string name) =>
        type.Fields.FirstOrDefault(f => f.Name == name)
        ?? throw new InvalidOperationException($"Required field not found: {type.FullName}::{name}");

    private static MethodDefinition RequireMethod(TypeDefinition type, string name, int? parameterCount = null)
    {
        var matches = type.Methods.Where(m => m.Name == name);
        if (parameterCount.HasValue)
            matches = matches.Where(m => m.Parameters.Count == parameterCount.Value);

        var list = matches.ToList();
        if (list.Count != 1)
            throw new InvalidOperationException(
                $"Expected one method {type.FullName}::{name}, found {list.Count}");
        return list[0];
    }

    private static void AddPlayerThreeEnum(TypeDefinition playerId)
    {
        var existing = playerId.Fields.FirstOrDefault(f => f.Name == "PlayerThree");
        if (existing != null)
        {
            if (!Equals(existing.Constant, 2))
                throw new InvalidOperationException("PlayerThree already exists but is not value 2");
            Console.WriteLine("[OK] PlayerId.PlayerThree already present");
            return;
        }

        var field = new FieldDefinition(
            "PlayerThree",
            FieldAttributes.Public | FieldAttributes.Static |
            FieldAttributes.Literal | FieldAttributes.HasDefault,
            playerId)
        {
            Constant = 2
        };
        playerId.Fields.Add(field);
        Console.WriteLine("[PATCH] PlayerId.PlayerThree = 2");
    }

    private static FieldDefinition AddMultiplayer2(TypeDefinition playerManager)
    {
        var existing = playerManager.Fields.FirstOrDefault(f => f.Name == "Multiplayer2");
        if (existing != null)
        {
            if (existing.FieldType.MetadataType != MetadataType.Boolean)
                throw new InvalidOperationException("PlayerManager.Multiplayer2 exists with unexpected type");
            Console.WriteLine("[OK] PlayerManager.Multiplayer2 already present");
            return existing;
        }

        var field = new FieldDefinition(
            "Multiplayer2",
            FieldAttributes.Public | FieldAttributes.Static,
            playerManager.Module.TypeSystem.Boolean);
        playerManager.Fields.Add(field);
        Console.WriteLine("[PATCH] PlayerManager.Multiplayer2");
        return field;
    }

    private static void PatchInputStart(TypeDefinition inputType)
    {
        var method = RequireMethod(inputType, "Start", 0);
        var body = method.Body;
        body.SimplifyMacrosSafe();
        var il = body.GetILProcessor();

        var playerControllers = RequireField(inputType, "playerControllers");
        var disconnected = RequireField(inputType, "disconnected");

        var controllersStore = FindStore(body, playerControllers);
        var controllersNewarr = FindPrevious(controllersStore, i => i.OpCode == OpCodes.Newarr,
            "Input.playerControllers newarr");
        SetPreviousArrayLength(controllersNewarr, 3, "Input.playerControllers");

        // Original initialization explicitly writes None to indexes 0 and 1.
        // Add the same initialization for P3; otherwise enum default 0 would look like GamePad.
        InsertBefore(il, controllersStore,
            il.Create(OpCodes.Dup),
            il.Create(OpCodes.Ldc_I4_2),
            il.Create(OpCodes.Ldc_I4_5),
            il.Create(OpCodes.Stelem_I4));

        var disconnectedStore = FindStore(body, disconnected);
        var disconnectedNewarr = FindPrevious(disconnectedStore, i => i.OpCode == OpCodes.Newarr,
            "Input.disconnected newarr");
        SetPreviousArrayLength(disconnectedNewarr, 3, "Input.disconnected");

        body.MaxStackSize = Math.Max(body.MaxStackSize, 6);
        Console.WriteLine("[PATCH] Input.Start arrays 2 -> 3");
    }

    private static void PatchPlayerManagerCctor(
        TypeDefinition playerManager,
        TypeDefinition playerId,
        FieldDefinition multiplayer2)
    {
        var method = RequireMethod(playerManager, ".cctor", 0);
        var body = method.Body;
        body.SimplifyMacrosSafe();
        var il = body.GetILProcessor();

        var playerSlots = RequireField(playerManager, "playerSlots");
        var validIds = RequireField(playerManager, "validIDs");
        var playerWasChalice = RequireField(playerManager, "playerWasChalice");
        var multiplayer = RequireField(playerManager, "Multiplayer");

        var slotType = playerManager.NestedTypes.FirstOrDefault(t => t.Name == "PlayerSlot")
            ?? throw new InvalidOperationException("PlayerManager.PlayerSlot not found");
        var slotCtor = slotType.Methods.Single(m => m.IsConstructor && !m.IsStatic && m.Parameters.Count == 0);

        var slotsStore = FindStore(body, playerSlots);
        var slotsNewarr = FindPrevious(slotsStore, i => i.OpCode == OpCodes.Newarr,
            "PlayerManager.playerSlots newarr");
        SetPreviousArrayLength(slotsNewarr, 3, "PlayerManager.playerSlots");
        InsertBefore(il, slotsStore,
            il.Create(OpCodes.Dup),
            il.Create(OpCodes.Ldc_I4_2),
            il.Create(OpCodes.Newobj, slotCtor),
            il.Create(OpCodes.Stelem_Ref));

        var idsStore = FindStore(body, validIds);
        var idsNewarr = FindPrevious(idsStore, i => i.OpCode == OpCodes.Newarr,
            "PlayerManager.validIDs newarr");
        SetPreviousArrayLength(idsNewarr, 3, "PlayerManager.validIDs");
        InsertBefore(il, idsStore,
            il.Create(OpCodes.Dup),
            il.Create(OpCodes.Ldc_I4_2),
            il.Create(OpCodes.Ldc_I4_2),
            il.Create(OpCodes.Stelem_I4));

        var chaliceStore = FindStore(body, playerWasChalice);
        var chaliceNewarr = FindPrevious(chaliceStore, i => i.OpCode == OpCodes.Newarr,
            "PlayerManager.playerWasChalice newarr");
        SetPreviousArrayLength(chaliceNewarr, 3, "PlayerManager.playerWasChalice");

        var multiplayerStore = FindStore(body, multiplayer);
        var afterMultiplayer = multiplayerStore.Next
            ?? throw new InvalidOperationException("Unexpected end of PlayerManager..cctor");
        InsertBefore(il, afterMultiplayer,
            il.Create(OpCodes.Ldc_I4_0),
            il.Create(OpCodes.Stsfld, multiplayer2));

        body.MaxStackSize = Math.Max(body.MaxStackSize, 6);
        Console.WriteLine("[PATCH] PlayerManager .cctor arrays 2 -> 3 + Multiplayer2");
    }

    private static void PatchPlayerManagerAwake(TypeDefinition playerManager, FieldDefinition multiplayer2)
    {
        var method = RequireMethod(playerManager, "Awake", 0);
        var body = method.Body;
        body.SimplifyMacrosSafe();
        var il = body.GetILProcessor();

        var multiplayer = RequireField(playerManager, "Multiplayer");
        var players = RequireField(playerManager, "players");
        var playerSlots = RequireField(playerManager, "playerSlots");
        var slotType = playerManager.NestedTypes.Single(t => t.Name == "PlayerSlot");
        var joinState = RequireField(slotType, "joinState");

        var multiplayerStore = FindStore(body, multiplayer);
        var afterMultiplayer = multiplayerStore.Next
            ?? throw new InvalidOperationException("Unexpected PlayerManager.Awake body");
        InsertBefore(il, afterMultiplayer,
            il.Create(OpCodes.Ldc_I4_0),
            il.Create(OpCodes.Stsfld, multiplayer2));

        var addCalls = body.Instructions
            .Where(i => i.OpCode == OpCodes.Callvirt &&
                        i.Operand is MethodReference mr &&
                        mr.Name == "Add")
            .ToList();
        if (addCalls.Count < 2)
            throw new InvalidOperationException("Could not locate PlayerManager.players dictionary Add calls");

        var addMethod = (MethodReference)addCalls[1].Operand;
        var insertAfterSecondAdd = addCalls[1].Next
            ?? throw new InvalidOperationException("Unexpected PlayerManager.Awake after second dictionary Add");

        InsertBefore(il, insertAfterSecondAdd,
            il.Create(OpCodes.Ldsfld, players),
            il.Create(OpCodes.Ldc_I4_2),
            il.Create(OpCodes.Ldnull),
            il.Create(OpCodes.Callvirt, addMethod));

        var slotOneStore = body.Instructions.LastOrDefault(i =>
            i.OpCode == OpCodes.Stfld &&
            i.Operand is FieldReference fr &&
            fr.Name == joinState.Name)
            ?? throw new InvalidOperationException("Could not locate slot joinState initialization");

        var tail = slotOneStore.Next
            ?? throw new InvalidOperationException("Unexpected PlayerManager.Awake tail");

        InsertBefore(il, tail,
            il.Create(OpCodes.Ldsfld, playerSlots),
            il.Create(OpCodes.Ldc_I4_2),
            il.Create(OpCodes.Ldelem_Ref),
            il.Create(OpCodes.Ldc_I4_0),
            il.Create(OpCodes.Stfld, joinState));

        body.MaxStackSize = Math.Max(body.MaxStackSize, 5);
        Console.WriteLine("[PATCH] PlayerManager.Awake registers P3 and initializes slot 2");
    }

    private static void PatchLevelCreatePlayers(TypeDefinition level)
    {
        var method = RequireMethod(level, "CreatePlayers", 0);
        var body = method.Body;
        body.SimplifyMacrosSafe();
        var il = body.GetILProcessor();

        var players = RequireField(level, "players");
        var blockChaliceField = RequireField(level, "blockChalice");
        var blockChaliceProperty = level.Properties.FirstOrDefault(p => p.Name == "BlockChaliceCharm")
            ?? throw new InvalidOperationException("Level.BlockChaliceCharm property not found");
        var getBlockChalice = blockChaliceProperty.GetMethod
            ?? throw new InvalidOperationException("Level.BlockChaliceCharm getter not found");
        var setBlockChalice = blockChaliceProperty.SetMethod
            ?? throw new InvalidOperationException("Level.BlockChaliceCharm setter not found");

        var playersStore = body.Instructions.FirstOrDefault(i =>
            i.OpCode == OpCodes.Stfld &&
            i.Operand is FieldReference fr &&
            fr.Name == players.Name)
            ?? throw new InvalidOperationException("Level.players store not found");
        var playersNewarr = FindPrevious(playersStore, i => i.OpCode == OpCodes.Newarr,
            "Level.players newarr");
        SetPreviousArrayLength(playersNewarr, 3, "Level.players");

        var setCall = body.Instructions.FirstOrDefault(i =>
            (i.OpCode == OpCodes.Call || i.OpCode == OpCodes.Callvirt) &&
            i.Operand is MethodReference mr &&
            mr.Name == setBlockChalice.Name)
            ?? throw new InvalidOperationException("BlockChaliceCharm setter call not found");
        var blockNewarr = FindPrevious(setCall, i => i.OpCode == OpCodes.Newarr,
            "Level.BlockChaliceCharm newarr");
        SetPreviousArrayLength(blockNewarr, 3, "Level.BlockChaliceCharm");

        var stelemOnes = body.Instructions
            .TakeWhile(i => i != body.Instructions.FirstOrDefault(x =>
                x.OpCode == OpCodes.Ldfld &&
                x.Operand is FieldReference fr &&
                fr.Name == "playerMode"))
            .Where(i => i.OpCode == OpCodes.Stelem_I1)
            .ToList();

        if (stelemOnes.Count < 2)
            throw new InvalidOperationException("Could not locate initial BlockChaliceCharm assignments");

        var afterSecond = stelemOnes[1].Next
            ?? throw new InvalidOperationException("Unexpected Level.CreatePlayers body");

        InsertBefore(il, afterSecond,
            il.Create(OpCodes.Ldarg_0),
            il.Create(OpCodes.Call, getBlockChalice),
            il.Create(OpCodes.Ldc_I4_2),
            il.Create(OpCodes.Ldarg_0),
            il.Create(OpCodes.Ldfld, blockChaliceField),
            il.Create(OpCodes.Stelem_I1));

        body.MaxStackSize = Math.Max(body.MaxStackSize, 6);
        Console.WriteLine("[PATCH] Level.CreatePlayers arrays 2 -> 3");
    }

    private static Instruction FindStore(MethodBody body, FieldDefinition field) =>
        body.Instructions.FirstOrDefault(i =>
            i.OpCode == OpCodes.Stsfld &&
            i.Operand is FieldReference fr &&
            fr.Name == field.Name &&
            fr.DeclaringType.Name == field.DeclaringType.Name)
        ?? throw new InvalidOperationException(
            $"Store not found for {field.DeclaringType.FullName}::{field.Name}");

    private static Instruction FindPrevious(
        Instruction start,
        Func<Instruction, bool> predicate,
        string label)
    {
        for (var current = start.Previous; current != null; current = current.Previous)
        {
            if (predicate(current))
                return current;
        }

        throw new InvalidOperationException($"Could not locate {label}");
    }

    private static void SetPreviousArrayLength(Instruction newarr, int length, string label)
    {
        var ldc = newarr.Previous
            ?? throw new InvalidOperationException($"Missing array length for {label}");

        if (!TryReadInt32(ldc, out var oldLength))
            throw new InvalidOperationException($"Unexpected array length opcode for {label}: {ldc.OpCode}");

        if (oldLength != 2 && oldLength != 3)
            throw new InvalidOperationException($"Unexpected {label} length: {oldLength}");

        if (oldLength == 3)
        {
            Console.WriteLine($"[OK] {label} already length 3");
            return;
        }

        SetLdcI4(ldc, length);
    }

    private static bool TryReadInt32(Instruction instruction, out int value)
    {
        value = 0;
        if (instruction.OpCode == OpCodes.Ldc_I4_M1) { value = -1; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_0) { value = 0; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_1) { value = 1; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_2) { value = 2; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_3) { value = 3; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_4) { value = 4; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_5) { value = 5; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_6) { value = 6; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_7) { value = 7; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_8) { value = 8; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4_S && instruction.Operand is sbyte sb)
        { value = sb; return true; }
        if (instruction.OpCode == OpCodes.Ldc_I4 && instruction.Operand is int i)
        { value = i; return true; }
        return false;
    }

    private static void SetLdcI4(Instruction instruction, int value)
    {
        instruction.Operand = null;
        instruction.OpCode = value switch
        {
            -1 => OpCodes.Ldc_I4_M1,
            0 => OpCodes.Ldc_I4_0,
            1 => OpCodes.Ldc_I4_1,
            2 => OpCodes.Ldc_I4_2,
            3 => OpCodes.Ldc_I4_3,
            4 => OpCodes.Ldc_I4_4,
            5 => OpCodes.Ldc_I4_5,
            6 => OpCodes.Ldc_I4_6,
            7 => OpCodes.Ldc_I4_7,
            8 => OpCodes.Ldc_I4_8,
            _ => OpCodes.Ldc_I4
        };
        if (instruction.OpCode == OpCodes.Ldc_I4)
            instruction.Operand = value;
    }

    private static void InsertBefore(ILProcessor il, Instruction target, params Instruction[] instructions)
    {
        foreach (var instruction in instructions)
            il.InsertBefore(target, instruction);
    }

    // Mono.Cecil.Rocks isn't required for this patcher; this keeps the source
    // independent of the optional Rocks package. Current targets already use
    // concrete instruction forms that can be patched directly.
    private static void SimplifyMacrosSafe(this MethodBody body)
    {
        if (!body.HasInstructions)
            throw new InvalidOperationException("Method has no IL body");
    }
}
