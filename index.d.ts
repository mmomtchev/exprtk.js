export type TypedArray = Int8Array | Uint8Array | Int16Array | Uint16Array | Int32Array | Uint32Array | Float32Array | Float64Array;

export type TypedArrayType = 'Int8' | 'Uint8' | 'Int16' | 'Uint16' | 'Int32' | 'Uint32' | 'Float32' | 'Float64';

export class Expression<T> {
  constructor(expression: string, scalars?: string[], vectors?: Record<string, number>);

  static readonly maxParallel: number;

  readonly expression: string;
  readonly type: TypedArrayType;
  readonly scalars: string[];
  readonly vectors: Record<string, number>;
  maxParallel: number;
  readonly maxActive: number;

  eval(arguments: Record<string, number | T>): number;
  eval(...arguments: (number | T)[]): number;

  evalAsync(arguments: Record<string, number | T>): Promise<number>;
  evalAsync(...arguments: (number | T)[]): Promise<number>;
  evalAsync(arguments: Record<string, number | T>, callback: (this: Expression<T>, e: Error | null, r: number | undefined) => void): void;


  map(array: T, iterator: string, arguments: Record<string, number | T>): T;
  map(array: T, iterator: string, ...arguments: (number | T)[]): T;
  map(target: T, array: T, iterator: string, arguments: Record<string, number | T>): T;
  map(target: T, array: T, iterator: string, ...arguments: (number | T)[]): T;

  map(threads: number, array: T, iterator: string, arguments: Record<string, number | T>): T;
  map(threads: number, array: T, iterator: string, ...arguments: (number | T)[]): T;
  map(threads: number, target: T, array: T, iterator: string, arguments: Record<string, number | T>): T;
  map(threads: number, target: T, array: T, iterator: string, ...arguments: (number | T)[]): T;

  mapAsync(array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapAsync(array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: Expression<T>, e: Error | null, r: T | undefined) => void): void;

  mapAsync(target: T, array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(target: T, array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapAsync(target: T, array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: Expression<T>, e: Error | null, r: T | undefined) => void): void;

  mapAsync(threads: number, array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(threads: number, array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapAsync(threads: number, array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: Expression<T>, e: Error | null, r: T | undefined) => void): void;

  mapAsync(threads: number, target: T, array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(threads: number, target: T, array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapAsync(threads: number, target: T, array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: Expression<T>, e: Error | null, r: T | undefined) => void): void;


  mapMPAsync(array: T, threads: number, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapMPAsync(array: T, threads: number, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapMPAsync(target: T, array: T, threads: number, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapMPAsync(target: T, array: T, threads: number, iterator: string, ...arguments: (number | T)[]): Promise<T>;


  reduce(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>): number;
  reduce(array: T, iterator: string, accumulator: string, initializer: number, ...arguments: (number | T)[]): number;

  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>): Promise<number>;
  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, ...arguments: (number | T)[]): Promise<number>;
  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>, callback: (this: Expression<T>, e: Error | null, r: number | undefined) => void): void

  cwise(arguments: Record<string, number | TypedArray>): T;
  cwise<U extends TypedArray>(arguments: Record<string, number | TypedArray>, result: U): U;

  cwiseAsync(arguments: Record<string, number | TypedArray>): Promise<T>;
  cwiseAsync<U extends TypedArray>(arguments: Record<string, number | TypedArray>, result: U): Promise<U>;
  cwiseAsync(arguments: Record<string, number | TypedArray>, callback: (this: Expression<T>, e: Error | null, r: T | undefined) => void): void;
}

export class Int8 extends Expression<Int8Array>{ }
export class Uint8 extends Expression<Uint8Array>{ }
export class Int16 extends Expression<Int16Array>{ }
export class Uint16 extends Expression<Uint16Array>{ }
export class Int32 extends Expression<Int32Array>{ }
export class Uint32 extends Expression<Uint32Array>{ }
export class Float32 extends Expression<Float32Array>{ }
export class Float64 extends Expression<Float64Array>{ }
