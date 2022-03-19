#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/DiagnosticInfo.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Error.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Analysis/AliasAnalysis.h"



using namespace llvm;

extern "C" int f1(int i, void *p) {
  printf("ole!! %d %p\n", i, p);
  using ff = int(*)(int);
  ff z = (ff)p;
  printf("oo %d\n", z(90));
  return 10;
}
struct c1 {
  void meth() {
    printf("method called\n");
  }
};
c1 c1inst;
extern "C" int method(c1 *i) {
  i->meth();
  return 11;
}

void optimize(std::unique_ptr<Module> &module) {
  PassBuilder pb;
  legacy::PassManager pm;
  PassManagerBuilder builder;
  builder.OptLevel = 3;
  builder.Inliner = createFunctionInliningPass(3, 0, false);
  builder.LoopVectorize = true;
  builder.SLPVectorize = true;

  builder.populateModulePassManager(pm);

  llvm::PassManagerBuilder lbuilder;
  lbuilder.VerifyInput = true;
  lbuilder.Inliner = llvm::createFunctionInliningPass(3, 0, false);
  lbuilder.populateLTOPassManager(pm);

  pm.run(*module);
}
void optimize2(std::unique_ptr<Module> &module) {
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;
  PassBuilder PB;
  FAM.registerPass([&] {return PB.buildDefaultAAPipeline(); });
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
  ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(PassBuilder::OptimizationLevel::O2);
  MPM.run(*module.get(), MAM);
}

// no arguments; return constant
Function* func1(std::unique_ptr<Module> &mod, LLVMContext &context) {
  std::vector<Type*> args;
  FunctionType *FT = FunctionType::get(Type::getInt32Ty(context), args, false);
  Function *F = Function::Create(FT, Function::PrivateLinkage, "func1", mod.get());
  ConstantInt *three = ConstantInt::get(IntegerType::getInt32Ty(context), 10);
  three->setName("three");

  BasicBlock* block = BasicBlock::Create(context, "entry", F);

  IRBuilder<> builder(block);
  builder.CreateRet(three);
  return F;
}

//return arg + 10
Function* func2(std::unique_ptr<Module> &mod, LLVMContext &context) {
  std::vector<Type*> args;
  args.push_back(Type::getInt32Ty(context));
  FunctionType *FT = FunctionType::get(Type::getInt32Ty(context), args, false);
  Function *F = Function::Create(FT, Function::PrivateLinkage, "func2", mod.get());
  ConstantInt *three = ConstantInt::get(IntegerType::getInt32Ty(context), 10);

  Function::arg_iterator argz = F->arg_begin();
  Value* x = argz++;
  x->setName("x");

  BasicBlock* block = BasicBlock::Create(context, "entry", F);


  IRBuilder<> builder(block);
  Value *out = builder.CreateAdd(x, three);
  builder.CreateRet(out);
  return F;
}

std::unique_ptr<Module>loadModule(LLVMContext &context) {
  SMDiagnostic Err;
  std::unique_ptr<Module> mod = std::make_unique<Module>("test", context);

  Function *func1c = func1(mod, context);
  Function *func2c = func2(mod, context);

  std::vector<Type*> args(1, Type::getInt32Ty(context));
  args.push_back(Type::getInt8PtrTy(context));
  FunctionType *FT = FunctionType::get(Type::getInt32Ty(context), args, false);

  Function *F = Function::Create(FT, Function::ExternalLinkage, "main", mod.get());

  Function::arg_iterator argz = F->arg_begin();
  Value* x = argz++;
  x->setName("x");

  Value* inst = argz++;
  inst->setName("inst");

  BasicBlock* block = BasicBlock::Create(context, "entry", F);

  std::vector<Type*> argsMeth(1, Type::getInt8PtrTy(context));
  FunctionType *FTMeth = FunctionType::get(Type::getInt32Ty(context), argsMeth, false);
  Function *FMeth = Function::Create(FTMeth, Function::ExternalLinkage, "method", mod.get());



  std::vector<Type*> argsA(1, Type::getInt32Ty(context));
  FunctionType *FTA = FunctionType::get(Type::getInt32Ty(context), argsA, false);
  //const char *name = "_Z2f1i";
  const char *name = "f1";
  Function *FA = Function::Create(FTA, Function::ExternalLinkage, name, mod.get());

  printf("%p\n", FA);


  IRBuilder<> builder(block);
  {
     std::vector<Value*> args1;
     args1.push_back(inst);
     Value *callret = builder.CreateCall(FMeth, args1);
   
  }

  std::vector<Value*> args1;
  args1.push_back(x);
  args1.push_back(func2c);
  Value *callret = builder.CreateCall(FA, args1);
  callret->setName("callret");

  ConstantInt *one = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

  Value *v1 = builder.CreateCall(func1c);
  Value *addret = builder.CreateAdd(callret, one);
  Value *addret2 = builder.CreateAdd(addret, v1);
  builder.CreateRet(addret2);

  return std::move(mod);
}

Error jitmain(std::unique_ptr<Module> M, std::unique_ptr<LLVMContext> Ctx) {
  auto JIT = orc::LLJITBuilder().create();
  if (!JIT) return JIT.takeError();

  if (auto Err = (*JIT)->addIRModule(orc::ThreadSafeModule(std::move(M), std::move(Ctx))))
    return Err;

  const DataLayout &DL = (*JIT)->getDataLayout();
  auto DLSG = orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix());
  if (!DLSG) return DLSG.takeError();

  (*JIT)->getMainJITDylib().addGenerator(std::move(*DLSG));

  auto MainSym = (*JIT)->lookup("main");
  if (!MainSym) return MainSym.takeError();

  auto *Main = (int (*)(int, c1*))MainSym->getAddress();

  int a = Main(2, &c1inst);
  printf("%d\n", a);
  return Error::success();
}

int main(int argc, char *argv[]) {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();


  auto Ctx = std::make_unique<LLVMContext>();

  std::unique_ptr<Module> M = loadModule(*Ctx);
  M->print(outs(), nullptr);
  optimize2(M);
  printf("-----------------------------------\n");
  M->print(outs(), nullptr);

  Error err = jitmain(std::move(M), std::move(Ctx));
  if (err) {
    std::string s = toString(std::move(err));
    printf("err: %s\n", s.c_str());
  }

  return 0;
}
