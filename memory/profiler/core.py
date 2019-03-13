
import enum
from typing import List
from io import StringIO

NEST_INDENT_STEP = 4

class HideFlags(enum.Enum):
    Clear, HideInHierarchy, HideInInspector, DontSaveInEditor, NotEditable, DontSaveInBuild, DontUnloadUnusedAsset= range(7)

class MemoryObject(object):

    def __format_bytes__(self, bytes:bytes):
        return '{:,}'.format(len(bytes) if bytes else 0)

    def dump(self, indent:str = ''):
        return indent

    def __repr__(self):
        return self.dump()

class Connection(MemoryObject):
    def __init__(self):
        super(Connection, self).__init__()
        self.from_:int = -1
        self.to:int = -1

    def dump(self, indent = ''):
        return '{}[Connection] from={} to={}'.format(indent, self.from_, self.to)

class FieldDescription(MemoryObject):
    def __init__(self):
        super(FieldDescription, self).__init__()
        self.isStatic:bool = False
        self.name:str = ''
        self.offset:int = -1
        self.typeIndex:int = -1

    def dump(self, indent:str = ''):
        return '{}[FieldDescription] isStatic={} name={} offset={} typeIndex={}'.format(indent, self.isStatic, self.name, self.offset, self.typeIndex)

class TypeDescription(MemoryObject):
    def __init__(self):
        super(TypeDescription, self).__init__()
        self.arrayRank:int = -1
        self.assembly:str = ''
        self.baseOrElementTypeIndex:int = -1
        self.fields:List[FieldDescription] = []
        self.isArray:bool = False
        self.isValueType:bool = False
        self.name:str = ''
        self.size:int = -1
        self.staticFieldBytes:bytes = None
        self.typeIndex:int = -1
        self.typeInfoAddress:int = 0 # pointer
        # extend fields
        self.instanceCount: int = 0
        self.instanceMemory: int = 0

    def dump(self, indent:str = ''):
        fp = StringIO()
        fp.write('{}[TypeDescription] arrayRank={} assembly={} baseOrElementTypeIndex={} isArray={} isValueType={} name={} size={} staticFieldBytes={} typeIndex={} typeInfoAddress={} instanceCount={} instanceMemory={}'.format(
            indent, self.arrayRank, self.assembly, self.baseOrElementTypeIndex, self.isArray, self.isValueType, self.name, self.size, self.__format_bytes__(self.staticFieldBytes), self.typeIndex, self.typeInfoAddress, self.instanceCount, self.instanceMemory
        ))
        nest_indent = indent + ' '*NEST_INDENT_STEP
        iter_count = len(self.fields)
        if iter_count > 0:
            fp.write('\n')
            for n in range(iter_count):
                field = self.fields[n]
                fp.write(field.dump(nest_indent))
                if n + 1 < iter_count: fp.write('\n')
        fp.seek(0)
        return fp.read()

class MemorySection(MemoryObject):
    def __init__(self):
        super(MemorySection, self).__init__()
        self.startAddress:int = 0 # pointer
        self.bytes:bytes = None

    def dump(self, indent:str = ''):
        return '{}[MemorySection] startAddress={:016x} bytes={}'.format(indent, self.startAddress, self.__format_bytes__(self.bytes))

class PackedGCHandle(MemoryObject):
    def __init__(self):
        super(PackedGCHandle, self).__init__()
        self.target:int = 0 # pointer

    def dump(self, indent:str = ''):
        return '{}[PackedGCHandle] target={:016x}'.format(indent, self.target)

class PackedNativeUnityEngineObject(MemoryObject):
    def __init__(self):
        super(PackedNativeUnityEngineObject, self).__init__()
        self.hideFlags:int = 0
        self.instanceId:int = -1
        self.isDontDestroyOnLoad:bool = False
        self.isManager:bool = False
        self.isPersistent:bool = False
        self.name:str = ''
        self.nativeObjectAddress:int = 0 # pointer
        self.nativeTypeArrayIndex:int = -1
        self.size:int= -1

    def __format_flags(self):
        shift = 0
        item_list = []
        while shift < 8:
            flag = self.hideFlags & (1 << shift)
            if flag != 0:
                item_list.append(HideFlags(shift + 1))
            shift += 1
        if not item_list: item_list.append(HideFlags.Clear)
        return '|'.join([x.name for x in item_list])

    def dump(self, indent:str = ''):
        return '{}[PackedNativeUnityEngineObject] hideFlags={:08b} instanceId={} isDontDestroyOnLoad={} isManager={} isPersistent={} name={} nativeObjectAddress={:016x} nativeTypeArrayIndex={} size={}'.format(
            indent, self.hideFlags, self.instanceId, self.isDontDestroyOnLoad, self.isManager, self.isPersistent, self.name, self.nativeObjectAddress, self.nativeTypeArrayIndex, self.size
        )

class PackedNativeType(MemoryObject):
    def __init__(self):
        super(PackedNativeType, self).__init__()
        self.name:str = ''
        self.nativeBaseTypeArrayIndex:int = -1
        # extend fields
        self.typeIndex:int = -1
        self.instanceCount:int = 0
        self.instanceMemory:int = 0

    def dump(self, indent:str = ''):
        return '{}[PackedNativeType] name={} nativeBaseTypeArrayIndex={} typeIndex={} instanceCount={} instanceMemory={}'.format(
            indent, self.name, self.nativeBaseTypeArrayIndex, self.typeIndex, self.instanceCount, self.instanceMemory
        )

class VirtualMachineInformation(MemoryObject):
    def __init__(self):
        super(VirtualMachineInformation, self).__init__()
        self.allocationGranularity:int = -1
        self.arrayBoundsOffsetInHeader:int = -1
        self.arrayHeaderSize:int = -1
        self.arraySizeOffsetInHeader:int = -1
        self.heapFormatVersion:int = -1
        self.objectHeaderSize:int = -1
        self.pointerSize:int = -1

    def dump(self, indent:str = ''):
        return '{}[VirtualMachineInformation] allocationGranularity={} arrayBoundsOffsetInHeader={} arrayHeaderSize={} arraySizeOffsetInHeader={} heapFormatVersion={} objectHeaderSize={} pointerSize={}'.format(
            indent, self.allocationGranularity, self.arrayBoundsOffsetInHeader, self.arrayHeaderSize, self.arraySizeOffsetInHeader, self.heapFormatVersion, self.objectHeaderSize, self.pointerSize
        )

class PackedMemorySnapshot(MemoryObject):
    def __init__(self):
        super(PackedMemorySnapshot, self).__init__()
        self.connections:List[Connection] = []
        self.gcHandles:List[PackedGCHandle] = []
        self.managedHeapSections:List[MemorySection] = []
        self.nativeObjects:List[PackedNativeUnityEngineObject] = []
        self.nativeTypes:List[PackedNativeType] = []
        self.typeDescriptions:List[TypeDescription] = []
        self.virtualMachineInformation:VirtualMachineInformation = None

    def dump(self, indent:str = ''):
        fp = StringIO()
        fp.write('{}[PackedMemorySnapshot]\n'.format(indent))
        indent_1 = indent + ' '*NEST_INDENT_STEP
        indent_2 = indent_1 + ' '*NEST_INDENT_STEP
        if self.connections:
            fp.write('{}connections=[Array]\n'.format(indent_1))
            iter_count = len(self.connections)
            for n in range(iter_count):
                it = self.connections[n]
                fp.write('{}\n'.format(it.dump(indent_2)))
        if self.gcHandles:
            fp.write('{}gcHandles=[Array]\n'.format(indent_1))
            iter_count = len(self.gcHandles)
            for n in range(iter_count):
                it = self.gcHandles[n]
                fp.write('{}\n'.format(it.dump(indent_2)))
        if self.managedHeapSections:
            fp.write('{}managedHeapSections=[Array]\n'.format(indent_1))
            iter_count = len(self.managedHeapSections)
            for n in range(iter_count):
                it = self.managedHeapSections[n]
                fp.write('{}\n'.format(it.dump(indent_2)))
        if self.nativeObjects:
            fp.write('{}nativeObjects=[Array]\n'.format(indent_1))
            iter_count = len(self.nativeObjects)
            for n in range(iter_count):
                it = self.nativeObjects[n]
                fp.write('{}\n'.format(it.dump(indent_2)))
        if self.typeDescriptions:
            fp.write('{}typeDescriptions=[Array]\n'.format(indent_1))
            iter_count = len(self.typeDescriptions)
            for n in range(iter_count):
                it = self.typeDescriptions[n]
                fp.write('{}\n'.format(it.dump(indent_2)))
        if self.virtualMachineInformation:
            fp.write('{}virtualMachineInformation=[Object]\n'.format(indent_1))
            fp.write(self.virtualMachineInformation.dump(indent_2))
        fp.seek(0)
        return fp.read()
