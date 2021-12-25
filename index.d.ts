export type TypedArray = Int8Array | Uint8Array | Int16Array | Uint16Array | Int32Array | Uint32Array | Float32Array | Float64Array;

export type TypedArrayType = 'Int8' | 'Uint8' | 'Int16' | 'Uint16' | 'Int32' | 'Uint32' | 'Float32' | 'Float64';

export class Expression<T> {
  constructor(expression: string, scalars?: string[], vectors?: Record<string, number>);

  readonly expression: string;
  readonly type: TypedArrayType;
  readonly scalars: string[];
  readonly vectors: Record<string, number>;


  eval(arguments: Record<string, number | T>): number;
  eval(...arguments: (number | T)[]): number;

  evalAsync(arguments: Record<string, number | T>): Promise<number>;
  evalAsync(...arguments: (number | T)[]): Promise<number>;


  map(array: T, iterator: string, arguments: Record<string, number | T>): T;
  map(array: T, iterator: string, ...arguments: (number | T)[]): T;

  mapAsync(array: T, iterator: string, arguments: Record<string, number | T>): Promise<T>;
  mapAsync(array: T, iterator: string, ...arguments: (number | T)[]): Promise<T>;


  reduce(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>): number;
  reduce(array: T, iterator: string, accumulator: string, initializer: number, ...arguments: (number | T)[]): number;

  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | T>): Promise<number>;
  reduceAsync(array: T, iterator: string, accumulator: string, initializer: number, ...arguments: (number | T)[]): Promise<number>;

  cwise(arguments: Record<string, number | TypedArray>): T;
  cwise<U extends TypedArray>(arguments: Record<string, number | TypedArray>, result: U): U;

  cwiseAsync(arguments: Record<string, number | TypedArray>): Promise<T>;
  cwiseAsync<U extends TypedArray>(arguments: Record<string, number | TypedArray>, result: U): Promise<U>;
}

export const Int8: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Int8Array>;
export const Uint8: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Uint8Array>;
export const Int16: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Int16Array>;
export const Uint16: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Uint16Array>;
export const Int32: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Int32Array>;
export const Uint32: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Uint32Array>;
export const Float32: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Float32Array>;
export const Float64: new (expression: string, scalars?: string[], vectors?: Record<string, number>) => Expression<Float64Array>;

export type Int8 = Expression<Int8Array>;
export type Uint8 = Expression<Uint8Array>;
export type Int16 = Expression<Int16Array>;
export type Uint16 = Expression<Uint16Array>;
export type Int32 = Expression<Int32Array>;
export type Uint32 = Expression<Uint32Array>;
export type Float32 = Expression<Float32Array>;
export type Float64 = Expression<Float64Array>;
