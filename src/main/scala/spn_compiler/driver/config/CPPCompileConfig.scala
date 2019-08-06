package spn_compiler.driver.config

import spn_compiler.driver.compile.cpu.{CPPCompilerDriver, ClangCPPDriver, GCCCPPDriver}

trait CPPCompileConfig[R <: CLIConfig[R]] extends CLIConfig[R] {

  private var compiler : CPPCompilerDriver = ClangCPPDriver
  def setCompiler(cxx : CPPCompilerDriver) : R = {
    compiler = cxx
    self
  }
  def compilerDriver : CPPCompilerDriver = compiler

  private var ompParFor : Boolean = false
  def enableOMPParallelFor(bool : Boolean) : R = {
    ompParFor = bool
    self
  }
  def isOMPParallelForEnabled : Boolean = ompParFor

  private var rangeProfiling : Boolean = false
  def enableRangeProfiling(bool : Boolean) : R = {
    rangeProfiling = bool
    self
  }
  def isRangeProfilingEnabled : Boolean = rangeProfiling

  private var lnsSim : Boolean = false
  def enableLNSSimulation(bool : Boolean) : R = {
    lnsSim = bool
    self
  }
  def isLNSSimulationEnabled : Boolean = lnsSim

  private var lnsIntBits : Int = 8
  def setLNSIntBits(bits : Int) : R = {
    lnsIntBits = bits
    self
  }
  def lnsIntegerBits : Int = lnsIntBits

  private var lnsFracBits : Int = 32
  def setLNSFractionBits(bits : Int) : R = {
    lnsFracBits = bits
    self
  }
  def lnsFractionBits : Int = lnsFracBits

  private var lnsError : Double = 1e-7
  def setLNSInterpolationError(error: Double) : R = {
    lnsError = error
    self
  }
  def lnsInterpolationError : Double = lnsError

  private var positSim : Boolean = false
  def enablePositSimulation(bool : Boolean) : R = {
    positSim = bool
    self
  }
  def isPositSimulationEnabled : Boolean = positSim

  private var positN : Int = 32
  def setPositSizeN(size : Int) : R = {
    positN = size
    self
  }
  def positSizeN : Int = positN

  private var positES : Int = 6
  def setPositSizeES(size : Int) : R = {
    positES = size
    self
  }
  def positSizeES : Int = positES

  private var fpSim : Boolean = false
  def enableFPSimulation(bool : Boolean) : R = {
    fpSim = bool
    self
  }
  def isFPSimulationEnabled : Boolean = fpSim

  private var fpMant : Int = 53
  def setFPMantissa(mantissa : Int ) : R = {
    fpMant = mantissa
    self
  }
  def fpMantissa : Int = fpMant

  private var fpExpMax : Int = 1000
  def setFPMaxExponent(max : Int) : R = {
    fpExpMax = max
    self
  }
  def fpMaxExponent : Int = fpExpMax

  private var fpExpMin : Int = -1000
  def setFPMinExponent(min : Int) : R = {
    fpExpMin = min
    self
  }
  def fpMinExponent : Int = fpExpMin

  private var fixedSim : Boolean = false
  def enableFixedPointSimulation(bool : Boolean) : R = {
    fixedSim = bool
    self
  }
  def isFixedPointSimulationEnabled : Boolean = fixedSim

  private var fixedIntBits : Int = 8
  def setFixedPointIntBits(bits : Int) : R = {
    fixedIntBits = bits
    self
  }
  def fixedPointIntegerBits : Int = fixedIntBits

  private var fixedFracBits : Int = 32
  def setFixedPointFractionBits(bits : Int) : R = {
    fixedFracBits = bits
    self
  }
  def fixedPointFractionBits : Int = fixedFracBits

}

private[driver] object CPPCompileConfig{
  def availableCompilers : List[(String, CPPCompilerDriver)] =
    List(ClangCPPDriver.compilerName, GCCCPPDriver.compilerName)
}