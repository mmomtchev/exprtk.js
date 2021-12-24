export type TypedArray<T> = Int8Array | Uint8Array | Int16Array | Uint16Array | Int32Array | Uint32Array | Float32Array | Float64Array;

export type TypedArrayType = 'Int8' | 'Uint8' | 'Int16' | 'Uint16' | 'Int32' | 'Uint32' | 'Float32' | 'Float64';

export class Expression<T> {
  constructor(expression: string, scalars?: string[], vectors?: Record<string, number>);

  readonly expression: string;
  readonly type: TypedArrayType;
  readonly scalars: string[];
  readonly vectors: Record<string, number>;


  eval(arguments: Record<string, number | TypedArray<T>>): number;
  eval(...arguments: (number | TypedArray<T>)[]): number;

  evalAsync(arguments: Record<string, number | TypedArray<T>>): Promise<number>;
  evalAsync(...arguments: (number | TypedArray<T>)[]): Promise<number>;


  map(array: TypedArray<T>, iterator: string, arguments: Record<string, number | TypedArray<T>>): TypedArray<T>;
  map(array: TypedArray<T>, iterator: string, ...arguments: (number | TypedArray<T>)[]): TypedArray<T>;

  mapAsync(array: TypedArray<T>, iterator: string, arguments: Record<string, number | TypedArray<T>>): Promise<TypedArray<T>>;
  mapAsync(array: TypedArray<T>, iterator: string, ...arguments: (number | TypedArray<T>)[]): Promise<TypedArray<T>>;


  reduce(array: TypedArray<T>, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | TypedArray<T>>): number;
  reduce(array: TypedArray<T>, iterator: string, accumulator: string, initializer: number, ...arguments: (number | TypedArray<T>)[]): number;

  reduceAsync(array: TypedArray<T>, iterator: string, accumulator: string, initializer: number, arguments: Record<string, number | TypedArray<T>>): Promise<number>;
  reduceAsync(array: TypedArray<T>, iterator: string, accumulator: string, initializer: number, ...arguments: (number | TypedArray<T>)[]): Promise<number>;


  cwise(arguments: Record<string, number | TypedArray<T>>, result?: TypedArray<T>): TypedArray<T>;

  cwiseAsync(arguments: Record<string, number | TypedArray<T>>, result?: TypedArray<T>): Promise<TypedArray<T>>;
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
export type Float64 = Expression<Float64Array>;
