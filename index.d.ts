export type TypedArray = Int8Array | Uint8Array | Int16Array | Uint16Array | Int32Array | Uint32Array | Float32Array | Float64Array;
export type TypedArrayType = 'Int8' | 'Uint8' | 'Int16' | 'Uint16' | 'Int32' | 'Uint32' | 'Float32' | 'Float64';
export type TypedArrayConstructor = Int8ArrayConstructor | Uint8ArrayConstructor | Int16ArrayConstructor |
  Uint16ArrayConstructor | Int32ArrayConstructor | Uint32ArrayConstructor |
  Float32ArrayConstructor | Float64ArrayConstructor;

export class Expression {}

export class TypedExpression<T extends TypedArray> extends Expression {
  constructor(expression: string, scalars?: string[], vectors?: Record<string, number>);

  static readonly maxParallel: number;

  readonly expression: string;
  static readonly type: TypedArrayType;
  readonly type: TypedArrayType;
  readonly scalars: string[];
  readonly vectors: Record<string, number>;
  maxParallel: number;
  readonly maxActive: number;
  static readonly allocator: TypedArrayConstructor;
  readonly allocator: TypedArrayConstructor;

  eval(arguments: Record<string, number | T>): number;
  eval(...arguments: (number | T)[]): number;

  evalAsync(arguments: Record<string, number | T>): Promise<number>;
  evalAsync(...arguments: (number | T)[]): Promise<number>;
  evalAsync(arguments: Record<string, number | T>, callback: (this: TypedExpression<T>, e: Error | null, r: number | undefined) => void): void;


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
  mapAsync(array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: TypedExpression<T>, e: Error | null, r: T | undefined) => void): void;

  mapAsync(target: T, array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(target: T, array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapAsync(target: T, array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: TypedExpression<T>, e: Error | null, r: T | undefined) => void): void;

  mapAsync(threads: number, array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(threads: number, array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapAsync(threads: number, array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: TypedExpression<T>, e: Error | null, r: T | undefined) => void): void;

  mapAsync(threads: number, target: T, array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(threads: number, target: T, array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;
  mapAsync(threads: number, target: T, array: T, iterator: string, arguments: Record<string, number | T>, callback: (this: TypedExpression<T>, e: Error | null, r: T | undefined) => void): void;


  reduce(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>): number;
  reduce(array: T, iterator: string, accumulator: string, initializer: number, ...arguments: (number | T)[]): number;

  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>): Promise<number>;
  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, ...arguments: (number | T)[]): Promise<number>;
  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>, callback: (this: TypedExpression<T>, e: Error | null, r: number | undefined) => void): void

  
  cwise(arguments: Record<string, number | TypedArray>): T;
  cwise<U extends TypedArray>(arguments: Record<string, number | TypedArray>, result: U): U;
  cwise(threads: number, arguments: Record<string, number | TypedArray>): T;
  cwise<U extends TypedArray>(threads: number, arguments: Record<string, number | TypedArray>, result: U): U;

  cwiseAsync(arguments: Record<string, number | TypedArray>): Promise<T>;
  cwiseAsync<U extends TypedArray>(arguments: Record<string, number | TypedArray>, result: U): Promise<U>;
  cwiseAsync(arguments: Record<string, number | TypedArray>, callback: (this: TypedExpression<T>, e: Error | null, r: T | undefined) => void): void;
  cwiseAsync(threads: number, arguments: Record<string, number | TypedArray>): Promise<T>;
  cwiseAsync<U extends TypedArray>(threads: number, arguments: Record<string, number | TypedArray>, result: U): Promise<U>;
  cwiseAsync(threads: number, arguments: Record<string, number | TypedArray>, callback: (this: TypedExpression<T>, e: Error | null, r: T | undefined) => void): void;
}

export class Int8 extends TypedExpression<Int8Array>{ }
export class Uint8 extends TypedExpression<Uint8Array>{ }
export class Int16 extends TypedExpression<Int16Array>{ }
export class Uint16 extends TypedExpression<Uint16Array>{ }
export class Int32 extends TypedExpression<Int32Array>{ }
export class Uint32 extends TypedExpression<Uint32Array>{ }
export class Float32 extends TypedExpression<Float32Array>{ }
export class Float64 extends TypedExpression<Float64Array>{ }

export type ExpressionConstructor = typeof Int8 | typeof Uint8 |
  typeof Int16 | typeof Uint16 |
  typeof Int32 | typeof Uint32 |
  typeof Float32 | typeof Float64;
